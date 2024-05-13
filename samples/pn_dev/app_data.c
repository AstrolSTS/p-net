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
#include <json.h>
#include <json_tokener.h>
#include <json_object.h>

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
static uint8_t counter = 0;

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

void init_kks_dcm(void) {
  
}

static void dump_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
   char *json_string;
	json_string = blobmsg_format_json_indent(msg, true, 0);

   struct json_object *json_obj = json_tokener_parse(json_string);
   struct json_object *array_obj;
   json_object_object_get_ex(json_obj, "", &array_obj);
   int array_len = json_object_array_length(array_obj);

   for (int i = 0; i < array_len; i++) {
      struct json_object *obj = json_object_array_get_idx(array_obj, i);

      int regidx;
      json_object_object_get_ex(obj, "regidx", &regidx);

      if (regidx == 14) {
         struct json_object *value_obj;
         json_object_object_get_ex(obj, "value", &value_obj);
         int value = json_object_get_int(value_obj);

         APP_LOG_FATAL("Value for regidx 14: %d", value);
        }
    }



	/*
   char *str;
	str = blobmsg_format_json_indent(msg, true, 0);
	APP_LOG_FATAL("Received data:\n%s\n", str);
	free(str);
   */

/*
   const struct blobmsg_policy result_attrs = {
		.name = "result", .type = BLOBMSG_TYPE_ARRAY
	};
   struct blob_attr *attr;
	//const char *data;

   blobmsg_parse(&result_attrs, 1, &attr, blobmsg_data(msg), blobmsg_len(msg));

   if (!attr) {
		APP_LOG_FATAL("Invalid argument\n");
	}
   */

/*
   const struct blobmsg_policy regname_attrs = {
		.name = "regname", .type = BLOBMSG_TYPE_STRING
	};

   struct blob_attr *reg_attr;
   blobmsg_parse(&regname_attrs, 1, &reg_attr, blobmsg_data(attr), blobmsg_len(attr));
*/
/*
   char *str;
   str = blobmsg_format_json_indent(attr, true, 0);
   APP_LOG_FATAL("Received data:\n%s\n", str);
   free(str);
*/

   

   

   //blobmsg_parse(&regname_attrs, 1, &attr, blobmsg_data(attr), blobmsg_len(attr));

/*
   struct blob_attr *last_attr;
	struct blobmsg_hdr *hdr;

   blob->head, blob_len(blob->head)

   __blob_for_each_attr(attr, msg->head, len) {
		hdr = blob_data(attr);
		if (!array)
			indent_printf(indent + 1, "%s : ", hdr->name);
		dump_attr_data(blobmsg_data(attr), blobmsg_data_len(attr), blob_id(attr), 0, indent + 1);
		last_attr = attr;
	}
*/

   //data = blobmsg_get_string(attr);
	//APP_LOG_FATAL("\nData: %s", data);

}

static int ubus_call(void) {
   const char *ubus_socket = NULL;
	uint32_t id;
   //char *result;

   APP_LOG_FATAL("\nubus_call");

   ctx = ubus_connect(ubus_socket);
   if (!ctx) {
      APP_LOG_FATAL("Failed to connect to ubus");
      return -1;
   }

   if (ubus_lookup_id(ctx, "kksdcmd", &id)) {
      APP_LOG_FATAL("Failed to lookup Ubus object");
      return -1;
   }
   blob_buf_init(&b,0);
   const char *method = "api";
   const char *parameter = "{\"coreregs\":{ \"generator\":\"1\",\"cmd\": \"read\", \"index\": 13, \"count\":5}}";

   blobmsg_add_json_from_string(&b, parameter);

   if(ubus_invoke(ctx, id, method, b.head, dump_cb, 0, 0)) {
      APP_LOG_FATAL("Failed to call ubus method %s", method);
   }

/*
   result = blobmsg_format_json(b.head, true);
   if (result) {
      APP_LOG_FATAL("System board info: %s", result);
      free(result);
   }
*/
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
      
      ubus_call();

      // KKS-DCM
      // Read generator data here
      // Parse and fill in into inputdata buffer
      for(i = 0;i<APP_NO_OF_GENERATORS;i++) {
         inputdata[(i*4)+0] = counter++;  // Generator x Status0
         inputdata[(i*4)+1] = counter++;  // Generator x Status1
         inputdata[(i*4)+2] = counter++;  // Generator x Error
         inputdata[(i*4)+3] = counter++;  // Generator x Actual Power
      }
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
            //outputdata[(i*3)+0]; // Generator x Control0
            //outputdata[(i*3)+1]; // Generator x Control1
            //outputdata[(i*3)+2]; // Generator x Power Set
         }  

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
