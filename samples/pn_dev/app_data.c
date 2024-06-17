/*********************************************************************
 *        _       _         _
 *  _ __ | |_  _ | |  __ _ | |__   ___
 * | '__|| __|(_)| | / _` || '_ \ / __|
 * | |   | |_  _ | || (_| || |_) |\__ \
 * |_|    \__|(_)|_| \__,_||_.__/ |___/
 *
 * www.rt-labs.com
 * Copyright 2021 rt-labs AB, Sweden.
 *
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 ********************************************************************/

#include "app_data.h"
#include "app_utils.h"
#include "app_gsdml.h"
#include "app_log.h"
#include "sampleapp_common.h"
#include "osal.h"
#include "pnal.h"
#include <pnet_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>

#define APP_DATA_DEFAULT_OUTPUT_DATA 0

/* Parameter data for digital submodules
 * The stored value is shared between all digital submodules in this example.
 *
 * Todo: Data is always in pnio data format. Add conversion to uint32_t.
 */
static uint32_t app_param_1 = 0; /* Network endianness */
static uint32_t app_param_2 = 0; /* Network endianness */

/* Parameter data for echo submodules
 * The stored value is shared between all echo submodules in this example.
 *
 * Todo: Data is always in pnio data format. Add conversion to uint32_t.
 */
static uint32_t app_param_echo_gain = 1; /* Network endianness */

/* Digital submodule process data
 * The stored value is shared between all digital submodules in this example. */
static uint8_t inputdata[APP_GSDML_INPUT_DATA_DIGITAL_SIZE] = {0};
static uint8_t outputdata[APP_GSDML_OUTPUT_DATA_DIGITAL_SIZE] = {0};
//static uint8_t counter = 0;

/* Network endianness */
static uint8_t echo_inputdata[APP_GSDML_INPUT_DATA_ECHO_SIZE] = {0};
static uint8_t echo_outputdata[APP_GSDML_OUTPUT_DATA_ECHO_SIZE] = {0};


CC_PACKED_BEGIN
typedef struct CC_PACKED app_echo_data
{
   /* Network endianness.
      Used as a float, but we model it as a 4-byte integer to easily
      do endianness conversion */
   uint32_t echo_float_bytes;

   /* Network endianness */
   uint32_t echo_int;
} app_echo_data_t;
CC_PACKED_END
CC_STATIC_ASSERT (sizeof (app_echo_data_t) == APP_GSDML_INPUT_DATA_ECHO_SIZE);
CC_STATIC_ASSERT (sizeof (app_echo_data_t) == APP_GSDML_OUTPUT_DATA_ECHO_SIZE);

static struct ubus_context *ctx;
static struct blob_buf b;
static bool initDone = false;
static uint32_t pnComSupervisor = 0;

typedef struct {
   uint8_t status0;
   uint8_t status1;
   uint8_t error;
   uint8_t actualPower;
   uint8_t control0;
   uint8_t control1;
   uint8_t powerSet;
   uint8_t enabled;
} GEN_DATA_TYPE;

static GEN_DATA_TYPE genData[APP_NO_OF_GENERATORS] = {0};

/**
 * Set LED state.
 *
 * Compares new state with previous state, to minimize system calls.
 *
 * Uses the hardware specific app_set_led() function.
 *
 * @param led_state        In:    New LED state
 */
static void app_handle_data_led_state (bool led_state)
{
   static bool previous_led_state = false;

   if (led_state != previous_led_state)
   {
      app_set_led (APP_DATA_LED_ID, led_state);
   }
   previous_led_state = led_state;
}

int init_kks_dcm(void) {
  const char *ubus_socket = NULL;
	uint32_t id;

   ctx = ubus_connect(ubus_socket);
   if (!ctx) {
      APP_LOG_FATAL("Failed to connect to ubus");
      return -1;
   }

   if (ubus_lookup_id(ctx, "kksdcmd", &id)) {
      APP_LOG_FATAL("Failed to lookup Ubus object");
      return -1;
   }

   const char *method = "api";
   char parameter[128];
   int16_t i;
   for(i=0;i<APP_NO_OF_GENERATORS;i++) {
      blob_buf_init(&b,0);
      sprintf(parameter,"{\"coreregs\":{ \"generator\":\"%d\",\"cmd\": \"list\", \"refresh\": true}}",i);
      blobmsg_add_json_from_string(&b, parameter);
      if(ubus_invoke(ctx, id, method, b.head, 0, 0, 0)) {
         APP_LOG_FATAL("Failed to call ubus method %s", method);
      }
      blob_buf_free(&b);
   }
   ubus_free(ctx);
   return 0;
}



static void read_gen_x(struct ubus_request *req, int type, struct blob_attr *msg, int genIndex)
{
   char *blobmsg_string;
	blobmsg_string = blobmsg_format_json_indent(msg, true, 0);

   json_object *root = json_tokener_parse(blobmsg_string);
   if (root == NULL) {
      APP_LOG_FATAL("Error parsing JSON");
   }
   
   json_object *result_array;
   genData[genIndex].status0 = 0;
   genData[genIndex].status1 = 0;
   genData[genIndex].error = 255;            // no communication
   genData[genIndex].actualPower = 0;
   genData[genIndex].enabled = 0;

   if (json_object_object_get_ex(root, "result", &result_array)) {
      int array_len = json_object_array_length(result_array);
      
      for (int i = 0; i < array_len; i++) {
         json_object *result_obj = json_object_array_get_idx(result_array, i);
         json_object *value_obj;
         if(json_object_object_get_ex(result_obj, "engval", &value_obj)) {
            if(i==0) {     // status0
               genData[genIndex].status0 = json_object_get_int(value_obj);
               genData[genIndex].enabled = 1;
            }
            if(i==1) {     // status1
               genData[genIndex].status1 = json_object_get_int(value_obj);
            }
            if(i==2) {     // error
               genData[genIndex].error = json_object_get_int(value_obj);
            }
            if(i==4) {     // actualPower
               genData[genIndex].actualPower = json_object_get_int(value_obj);
            }
            json_object_put(value_obj);
         }
         json_object_put(result_obj);
      }
   }
   json_object_put(result_array);   
   json_object_put(root);
   free(blobmsg_string);
}

static void read_gen_0(struct ubus_request *req, int type, struct blob_attr *msg) { read_gen_x(req,type,msg,0); }
static void read_gen_1(struct ubus_request *req, int type, struct blob_attr *msg) { read_gen_x(req,type,msg,1); }


static int ubus_call_read(void) {

   if(initDone == false) {       // initialize system on the first call
      if(0 == init_kks_dcm()) {
         initDone = true;
      }
   }

   const char *ubus_socket = NULL;
	uint32_t id;
   //char *result;

   //APP_LOG_FATAL("\nubus_call");

   ctx = ubus_connect(ubus_socket);
   if (!ctx) {
      APP_LOG_FATAL("Failed to connect to ubus");
      return -1;
   }

   if (ubus_lookup_id(ctx, "kksdcmd", &id)) {
      APP_LOG_FATAL("Failed to lookup Ubus object");
      return -1;
   }

   
   const char *method = "api";
   char parameter[128];
   int16_t i;
   for(i=0;i<APP_NO_OF_GENERATORS;i++) {
      blob_buf_init(&b,0);
      sprintf(parameter,"{\"coreregs\":{ \"generator\":\"%d\",\"cmd\": \"read\", \"index\": 13, \"count\":5, \"refresh\":true}}",i);
      blobmsg_add_json_from_string(&b, parameter);
      //APP_LOG_FATAL("\nGEN: %d", i);
      if(i == 0) { if(ubus_invoke(ctx, id, method, b.head, read_gen_0, 0, 0)) { APP_LOG_FATAL("Failed to call ubus method %s", method);}}
      if(i == 1) { if(ubus_invoke(ctx, id, method, b.head, read_gen_1, 0, 0)) { APP_LOG_FATAL("Failed to call ubus method %s", method);}}
      
      blob_buf_free(&b);
   }

   ubus_free(ctx);
   
   return 0;
}


static int ubus_call_write(void) {

   if(initDone == false) {       // initialize system on the first call
      if(0 == init_kks_dcm()) {
         initDone = true;
      }
   }

   const char *ubus_socket = NULL;
	uint32_t id;


   ctx = ubus_connect(ubus_socket);
   if (!ctx) {
      APP_LOG_FATAL("Failed to connect to ubus");
      return -1;
   }

   if (ubus_lookup_id(ctx, "kksdcmd", &id)) {
      APP_LOG_FATAL("Failed to lookup Ubus object");
      return -1;
   }

   
   const char *method = "api";
   char parameter[128];
   int16_t i,j,subCalls;
   for(i=0;i<APP_NO_OF_GENERATORS;i++) {
      if(i==0) {
         subCalls = 3;
      }
      else {
         subCalls = 4;
      }

      for(j=0;j<subCalls;j++) {
         if(genData[i].enabled) {
            blob_buf_init(&b,0);
            if(i==0) {    // master generator needs to commit every call
               if(j==0) { sprintf(parameter,"{\"coreregs\":{ \"generator\":\"%d\",\"cmd\": \"write\", \"index\": 32, \"value\":%d, \"commit\":true}}",i,genData[i].control0 );}
               if(j==1) { sprintf(parameter,"{\"coreregs\":{ \"generator\":\"%d\",\"cmd\": \"write\", \"index\": 33, \"value\":%d, \"commit\":true}}",i,genData[i].control1 );}
               if(j==2) { sprintf(parameter,"{\"coreregs\":{ \"generator\":\"%d\",\"cmd\": \"write\", \"index\": 34, \"value\":%d, \"commit\":true}}",i,genData[i].powerSet );}
            }
            else {      // master generator needs to commit after last call
               if(j==0) { sprintf(parameter,"{\"coreregs\":{ \"generator\":\"%d\",\"cmd\": \"write\", \"index\": 32, \"value\":%d, \"commit\":false}}",i,genData[i].control0 );}
               if(j==1) { sprintf(parameter,"{\"coreregs\":{ \"generator\":\"%d\",\"cmd\": \"write\", \"index\": 33, \"value\":%d, \"commit\":false}}",i,genData[i].control1 );}
               if(j==2) { sprintf(parameter,"{\"coreregs\":{ \"generator\":\"%d\",\"cmd\": \"write\", \"index\": 34, \"value\":%d, \"commit\":false}}",i,genData[i].powerSet );}
               if(j==3) { sprintf(parameter,"{\"coreregs\":{ \"generator\":\"%d\",\"cmd\": \"write\", \"index\": 32, \"count\":3, \"commit\":true}}",i);} 
            }
            blobmsg_add_json_from_string(&b, parameter);
            if(ubus_invoke(ctx, id, method, b.head, 0, 0, 0)) {
               APP_LOG_FATAL("Failed to call ubus method %s", method);
            }
            blob_buf_free(&b);
         }
      }
   }

   ubus_free(ctx);
   
   return 0;
}

static int ubus_call_write_pn_com_supervisor(void) {
   const char *ubus_socket = NULL;
	uint32_t id;

   ctx = ubus_connect(ubus_socket);
   if (!ctx) {
      APP_LOG_FATAL("Failed to connect to ubus");
      return -1;
   }

   if (ubus_lookup_id(ctx, "file", &id)) {
      APP_LOG_FATAL("Failed to lookup Ubus object");
      return -1;
   }

   const char *method = "write";
   char parameter[128];
   int16_t i,j,subCalls;

   blob_buf_init(&b,0);
   sprintf(parameter,"{\"path\":\"/root/pnComSupervisor\", \"data\":\"%d\"}",pnComSupervisor);
   blobmsg_add_json_from_string(&b, parameter);
   if(ubus_invoke(ctx, id, method, b.head, 0, 0, 0)) {
      APP_LOG_FATAL("Failed to call ubus method %s", method);
   }
   blob_buf_free(&b);
   ubus_free(ctx);
   return 0;
}

uint8_t * app_data_get_input_data (
   uint16_t slot_nbr,
   uint16_t subslot_nbr,
   uint32_t submodule_id,
   bool button_pressed,
   uint16_t * size,
   uint8_t * iops)
{
   float inputfloat;
   float outputfloat;
   uint32_t hostorder_inputfloat_bytes;
   uint32_t hostorder_outputfloat_bytes;
   app_echo_data_t * p_echo_inputdata = (app_echo_data_t *)&echo_inputdata;
   app_echo_data_t * p_echo_outputdata = (app_echo_data_t *)&echo_outputdata;
   int16_t i;

   if (size == NULL || iops == NULL)
   {
      return NULL;
   }

   if (
      submodule_id == APP_GSDML_SUBMOD_ID_DIGITAL_IN )
   {
 
      *size = APP_GSDML_INPUT_DATA_DIGITAL_SIZE;
      *iops = PNET_IOXS_GOOD;
      return inputdata;
   }

   if (
      submodule_id == APP_GSDML_SUBMOD_ID_DIGITAL_IN_OUT)
   {
      ubus_call_read();

      // KKS-DCM
      // Read generator data here
      // Parse and fill in into inputdata buffer
      pnComSupervisor &= ~(0xFFFF);                      // mask out active bits
      for(i = 0;i<APP_NO_OF_GENERATORS;i++) {
         inputdata[(i*4)+0] = genData[i].status0;        // Generator x Status0
         inputdata[(i*4)+1] = genData[i].status1;        // Generator x Status1
         inputdata[(i*4)+2] = genData[i].error;          // Generator x Error
         inputdata[(i*4)+3] = genData[i].actualPower;    // Generator x Actual Power

         if(genData[genIndex].error != 255) {
            pnComSupervisor |= (1 << i);
         }
      }
      pnComSupervisor += 0x10000;                        // higher word is used for supervisor counter
      ubus_call_write_pn_com_supervisor();
      *size = APP_GSDML_INPUT_DATA_DIGITAL_SIZE;
      *iops = PNET_IOXS_GOOD;
      return inputdata;
   }

   if (submodule_id == APP_GSDML_SUBMOD_ID_ECHO)
   {
      /* Calculate echodata input (to the PLC)
       * by multiplying the output (from the PLC) with a gain factor
       */

      /* Integer */
      p_echo_inputdata->echo_int = CC_TO_BE32 (
         CC_FROM_BE32 (p_echo_outputdata->echo_int) *
         CC_FROM_BE32 (app_param_echo_gain));

      /* Float */
      /* Use memcopy to avoid strict-aliasing rule warnings */
      hostorder_outputfloat_bytes =
         CC_FROM_BE32 (p_echo_outputdata->echo_float_bytes);
      memcpy (&outputfloat, &hostorder_outputfloat_bytes, sizeof (outputfloat));
      inputfloat = outputfloat * CC_FROM_BE32 (app_param_echo_gain);
      memcpy (&hostorder_inputfloat_bytes, &inputfloat, sizeof (outputfloat));
      p_echo_inputdata->echo_float_bytes =
         CC_TO_BE32 (hostorder_inputfloat_bytes);

      *size = APP_GSDML_INPUT_DATA_ECHO_SIZE;
      *iops = PNET_IOXS_GOOD;
      return echo_inputdata;
   }

   /* Automated RT Tester scenario 2 - unsupported (sub)module */
   return NULL;
}

int app_data_set_output_data (
   uint16_t slot_nbr,
   uint16_t subslot_nbr,
   uint32_t submodule_id,
   uint8_t * data,
   uint16_t size)
{
   bool led_state;
   int16_t i;

   if (data == NULL)
   {
      return -1;
   }

   if (
      submodule_id == APP_GSDML_SUBMOD_ID_DIGITAL_OUT ||
      submodule_id == APP_GSDML_SUBMOD_ID_DIGITAL_IN_OUT)
   {
      if (size == APP_GSDML_OUTPUT_DATA_DIGITAL_SIZE)
      {
         memcpy (outputdata, data, size);

         

         // KKS-DCM
         // Write data to generator here
         // read from outputdata buffer and fill into generator data
         for(i = 0;i<APP_NO_OF_GENERATORS;i++) {
            genData[i].control0 = outputdata[(i*3)+0]; // Generator x Control0
            genData[i].control1 = outputdata[(i*3)+1]; // Generator x Control1
            genData[i].powerSet = outputdata[(i*3)+2]; // Generator x Power Set
         }  

         ubus_call_write();

         /* Most significant bit: LED */
         led_state = 0;//(outputdata[0] & 0x80) > 0;
         app_handle_data_led_state (led_state);

         return 0;
      }
   }
   else if (submodule_id == APP_GSDML_SUBMOD_ID_ECHO)
   {
      if (size == APP_GSDML_OUTPUT_DATA_ECHO_SIZE)
      {
         memcpy (echo_outputdata, data, size);

         return 0;
      }
   }

   return -1;
}

int app_data_set_default_outputs (void)
{
   outputdata[0] = APP_DATA_DEFAULT_OUTPUT_DATA;
   app_handle_data_led_state (false);
   return 0;
}

int app_data_write_parameter (
   uint16_t slot_nbr,
   uint16_t subslot_nbr,
   uint32_t submodule_id,
   uint32_t index,
   const uint8_t * data,
   uint16_t length)
{
   const app_gsdml_param_t * par_cfg;

   par_cfg = app_gsdml_get_parameter_cfg (submodule_id, index);
   if (par_cfg == NULL)
   {
      APP_LOG_WARNING (
         "PLC write request unsupported submodule/parameter. "
         "Submodule id: %u Index: %u\n",
         (unsigned)submodule_id,
         (unsigned)index);
      return -1;
   }

   if (length != par_cfg->length)
   {
      APP_LOG_WARNING (
         "PLC write request unsupported length. "
         "Index: %u Length: %u Expected length: %u\n",
         (unsigned)index,
         (unsigned)length,
         par_cfg->length);
      return -1;
   }

   if (index == APP_GSDML_PARAMETER_1_IDX)
   {
      memcpy (&app_param_1, data, length);
   }
   else if (index == APP_GSDML_PARAMETER_2_IDX)
   {
      memcpy (&app_param_2, data, length);
   }
   else if (index == APP_GSDML_PARAMETER_ECHO_IDX)
   {
      memcpy (&app_param_echo_gain, data, length);
   }

   APP_LOG_DEBUG ("  Writing parameter \"%s\"\n", par_cfg->name);
   app_log_print_bytes (APP_LOG_LEVEL_DEBUG, data, length);

   return 0;
}

int app_data_read_parameter (
   uint16_t slot_nbr,
   uint16_t subslot_nbr,
   uint32_t submodule_id,
   uint32_t index,
   uint8_t ** data,
   uint16_t * length)
{
   const app_gsdml_param_t * par_cfg;

   par_cfg = app_gsdml_get_parameter_cfg (submodule_id, index);
   if (par_cfg == NULL)
   {
      APP_LOG_WARNING (
         "PLC read request unsupported submodule/parameter. "
         "Submodule id: %u Index: %u\n",
         (unsigned)submodule_id,
         (unsigned)index);
      return -1;
   }

   if (*length < par_cfg->length)
   {
      APP_LOG_WARNING (
         "PLC read request unsupported length. "
         "Index: %u Max length: %u Data length for our parameter: %u\n",
         (unsigned)index,
         (unsigned)*length,
         par_cfg->length);
      return -1;
   }

   APP_LOG_DEBUG ("  Reading \"%s\"\n", par_cfg->name);
   if (index == APP_GSDML_PARAMETER_1_IDX)
   {
      *data = (uint8_t *)&app_param_1;
      *length = sizeof (app_param_1);
   }
   else if (index == APP_GSDML_PARAMETER_2_IDX)
   {
      *data = (uint8_t *)&app_param_2;
      *length = sizeof (app_param_2);
   }
   else if (index == APP_GSDML_PARAMETER_ECHO_IDX)
   {
      *data = (uint8_t *)&app_param_echo_gain;
      *length = sizeof (app_param_echo_gain);
   }

   app_log_print_bytes (APP_LOG_LEVEL_DEBUG, *data, *length);

   return 0;
}
