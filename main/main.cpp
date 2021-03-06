/*
 * Copyright (C) 2017 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

/*
Channel 67
Tecla soltada: A1 01 00 00 00 00 00 00 00 00
NumLock:       A1 01 00 00 53 00 00 00 00 00
/:             A1 01 00 00 54 00 00 00 00 00
*:             A1 01 00 00 55 00 00 00 00 00
-:             A1 01 00 00 56 00 00 00 00 00 
7:             A1 01 00 00 5F 00 00 00 00 00 
8:             A1 01 00 00 60 00 00 00 00 00 
9:             A1 01 00 00 61 00 00 00 00 00 
+:             A1 01 00 00 57 00 00 00 00 00 
4:             A1 01 00 00 5C 00 00 00 00 00 
5:             A1 01 00 00 5D 00 00 00 00 00 
6:             A1 01 00 00 5E 00 00 00 00 00 
BackSpace:     A1 01 00 00 2A 00 00 00 00 00 
1:             A1 01 00 00 59 00 00 00 00 00 
2:             A1 01 00 00 5A 00 00 00 00 00 
3:             A1 01 00 00 5B 00 00 00 00 00 
Enter:         A1 01 00 00 58 00 00 00 00 00 
0:             A1 01 00 00 62 00 00 00 00 00
.:             A1 01 00 00 63 00 00 00 00 00 
*/

#include <inttypes.h>
#include <stdio.h>

#include "btstack_config.h"
#include "btstack.h"
#include "Arduino.h"
#include "IRremote.h"
#include "lg32ls570s.h"

#define DEBUG 0
#define debug(...) do { if(DEBUG) printf(__VA_ARGS__); } while (0)
#define debug_hexdump(...) do { if(DEBUG) printf_hexdump(__VA_ARGS__); } while (0)

#define MAX_ATTRIBUTE_VALUE_SIZE 300

// Device connection status
static uint8_t device_is_connected = 0;

// SDP
static uint8_t            hid_descriptor[MAX_ATTRIBUTE_VALUE_SIZE];
static uint16_t           hid_descriptor_len;

static uint16_t           hid_control_psm;
static uint16_t           hid_interrupt_psm;

static uint8_t            attribute_value[MAX_ATTRIBUTE_VALUE_SIZE];
static const unsigned int attribute_value_buffer_size = MAX_ATTRIBUTE_VALUE_SIZE;

// L2CAP
static uint16_t           l2cap_hid_control_cid;
static uint16_t           l2cap_hid_interrupt_cid;

// MBP 2016
static const char* remote_addr_string = "3D-0E-01-16-05-2E";

static bd_addr_t remote_addr;

static btstack_packet_callback_registration_t hci_event_callback_registration;

/**************************************************************************************************/

// Receive and Transmit pins
#define PIN_O_IR_TX 12

// Send a NEC code message
void ir_send_nec(const uint16_t code);

// IR Send Object
IRsend ir_tx(PIN_O_IR_TX);

// Send a NEC code message
void ir_send_nec(const uint16_t code)
{
    ir_tx.sendNEC(NEC_INIT_MASK | code, 32);
}

/**************************************************************************************************/

/* @section Main application configuration
 *
 * @text In the application configuration, L2CAP is initialized 
 */

/* LISTING_START(PanuSetup): Panu setup */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void hid_host_setup(void){

    // Initialize L2CAP 
    l2cap_init();

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // Allow sniff mode requests by HID device
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE);

    // Disable stdout buffering
    setbuf(stdout, NULL);
}
/* LISTING_END */

/* @section SDP parser callback 
 * 
 * @text The SDP parsers retrieves the BNEP PAN UUID as explained in  
 * Section [on SDP BNEP Query example](#sec:sdpbnepqueryExample}.
 */

static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    des_iterator_t attribute_list_it;
    des_iterator_t additional_des_it;
    des_iterator_t prot_it;
    uint8_t       *des_element;
    uint8_t       *element;
    uint32_t       uuid;
    uint8_t        status;

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= attribute_value_buffer_size) {
                attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);
                if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)) {
                    switch(sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                        case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {                                    
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                des_iterator_init(&prot_it, des_element);
                                element = des_iterator_get_element(&prot_it);
                                if (!element) continue;
                                if (de_get_element_type(element) != DE_UUID) continue;
                                uuid = de_get_uuid32(element);
                                des_iterator_next(&prot_it);
                                switch (uuid){
                                    case BLUETOOTH_PROTOCOL_L2CAP:
                                        if (!des_iterator_has_more(&prot_it)) continue;
                                        de_element_get_uint16(des_iterator_get_element(&prot_it), &hid_control_psm);
                                        debug("HID Control PSM: 0x%04x\n", (int) hid_control_psm);
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                        case BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {                                    
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                for (des_iterator_init(&additional_des_it, des_element); des_iterator_has_more(&additional_des_it); des_iterator_next(&additional_des_it)) {                                    
                                    if (des_iterator_get_type(&additional_des_it) != DE_DES) continue;
                                    des_element = des_iterator_get_element(&additional_des_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);
                                    if (!element) continue;
                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    uuid = de_get_uuid32(element);
                                    des_iterator_next(&prot_it);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &hid_interrupt_psm);
                                            debug("HID Interrupt PSM: 0x%04x\n", (int) hid_interrupt_psm);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            break;
                        case BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                for (des_iterator_init(&additional_des_it, des_element); des_iterator_has_more(&additional_des_it); des_iterator_next(&additional_des_it)) {                                    
                                    if (des_iterator_get_type(&additional_des_it) != DE_STRING) continue;
                                    element = des_iterator_get_element(&additional_des_it);
                                    const uint8_t * descriptor = de_get_string(element);
                                    hid_descriptor_len = de_get_data_size(element);
                                    memcpy(hid_descriptor, descriptor, hid_descriptor_len);
                                    debug("HID Descriptor:\n");
                                    debug_hexdump(hid_descriptor, hid_descriptor_len);
                                }
                            }                        
                            break;
                        default:
                            break;
                    }
                }
            } else {
                debug("SDP attribute value buffer size exceeded: available %d, required %d\n", attribute_value_buffer_size, sdp_event_query_attribute_byte_get_attribute_length(packet));
            }
            break;
            
        case SDP_EVENT_QUERY_COMPLETE:
            if (!hid_control_psm) {
                debug("HID Control PSM missing\n");
                break;
            }
            if (!hid_interrupt_psm) {
                debug("HID Interrupt PSM missing\n");
                break;
            }
            debug("Setup HID\n");
            status = l2cap_create_channel(packet_handler, remote_addr, hid_control_psm, 48, &l2cap_hid_control_cid);
            if (status){
                debug("Connecting to HID Control failed: 0x%02x\n", status);
            }
            break;
    }
}

char parse_numpad(const uint8_t val)
{
    if(val == 0x2A)
        return (char)(8); // BackSpace
    if(val == 0x54)
        return '/';
    if(val == 0x55)
        return '*';
    if(val == 0x56)
        return '-';
    if(val == 0x57)
        return '+';
    if(val == 0x58)
        return (char)(10); // Enter
    if(val == 0x59)
        return '1';
    if(val == 0x5A)
        return '2';
    if(val == 0x5B)
        return '3';
    if(val == 0x5C)
        return '4';
    if(val == 0x5D)
        return '5';
    if(val == 0x5E)
        return '6';
    if(val == 0x5F)
        return '7';
    if(val == 0x60)
        return '8';
    if(val == 0x61)
        return '9';
    if(val == 0x62)
        return '0';
    if(val == 0x63)
        return '.';

    return '\0';
}

/*
 * @section HID Report Handler
 * 
 * @text Use BTstack's compact HID Parser to process incoming HID Report
 * Iterate over all fields and process fields with usage page = 0x07 / Keyboard
 * Check if SHIFT is down and process first character (don't handle multiple key presses)
 * 
 */
static void hid_host_handle_interrupt_report(const uint8_t* report, uint16_t report_len)
{
    char c = '\0';

    // Ignore if report frame has an unwanted length (we want data frames with 10 bytes)
    if (report_len < 10)
        return;
    // Ignore if report frame doesn't start with expected values
    // "A1 01 00 00 KEY_1 KEY_2 KEY_3 KEY_4 KEY_5 00"
    if ((report[0] != 0xa1) || (report[1] != 0x01) || (report[2] != 0x00) || (report[3] != 0x00))
        return;
    // Ignore if report frame is for invalid number of keys pressed "A1 01 00 00 01 01 01 01 01 01"
    if ((report[0] == 0xa1) && (report[1] == 0x01) && (report[2] == 0x00) && (report[3] == 0x00) &&
        (report[4] == 0x01) && (report[5] == 0x01) && (report[6] == 0x01) && (report[7] == 0x01) &&
        (report[8] == 0x01) && (report[9] == 0x01))
    {
        return;
    }

    // Just keep keys data bytes from the frame (bypass "A1 01 00 00")
    report = report + 4;
    report_len = report_len - 4;

    // Detect all keys realeased
    if ((report[0] == 0x00) && (report[1] == 0x00) && (report[2] == 0x00) && 
        (report[3] == 0x00) && (report[4] == 0x00) && (report[5] == 0x00))
    {
        printf("Key: Released\n");
        return;
    }

    for(uint8_t i = 0; i < report_len; i++)
    {
        c = parse_numpad(report[i]);
        if(report[i] == 0x2A)
        {
            printf("Key: BackSpace\n");
            continue;
        }
        if(report[i] == 0x58)
        {
            printf("Key: Enter\n");
            continue;
        }
        if(report[i] != 0x00)
        {
            printf("Key: %c\n", c);
            if(c == '0')
                ir_send_nec(LG_NUMBER_0);
            else if(c == '1')
                ir_send_nec(LG_NUMBER_1);
            else if(c == '2')
                ir_send_nec(LG_NUMBER_2);
            else if(c == '3')
                ir_send_nec(LG_NUMBER_3);
            else if(c == '4')
                ir_send_nec(LG_NUMBER_4);
            else if(c == '5')
                ir_send_nec(LG_NUMBER_5);
            else if(c == '6')
                ir_send_nec(LG_NUMBER_6);
            else if(c == '7')
                ir_send_nec(LG_NUMBER_7);
            else if(c == '8')
                ir_send_nec(LG_NUMBER_8);
            else if(c == '9')
                ir_send_nec(LG_NUMBER_9);
            else if(c == '+')
                ir_send_nec(LG_VOL_PLUS);
            else if(c == '-')
                ir_send_nec(LG_VOL_LESS);
            else if(c == '*')
                ir_send_nec(LG_PROG_PLUS);
            else if(c == '/')
                ir_send_nec(LG_PROG_LESS);
            else if(c == '.')
                ir_send_nec(LG_POWER);
        }
    }
    printf("\n");
}

/*
 * @section Packet Handler
 * 
 * @text The packet handler responds to various HCI Events.
 */

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    /* LISTING_PAUSE */
    uint8_t   event;
    bd_addr_t event_addr;
    uint8_t   status;
    uint16_t  l2cap_cid;

    /* LISTING_RESUME */
    switch (packet_type) {
		case HCI_EVENT_PACKET:
            event = hci_event_packet_get_type(packet);
            printf("\n---Event received with code: 0x%02X---\n", event);
            switch (event) {
                /* @text When BTSTACK_EVENT_STATE with state HCI_STATE_WORKING
                 * is received and the example is started in client mode, the remote SDP HID query is started.
                 */
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                        debug("Start SDP HID query for remote HID Device.\n");
                        sdp_client_query_uuid16(&handle_sdp_client_query_result, remote_addr, BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
                    }
                    break;

                case HCI_EVENT_CONNECTION_COMPLETE:
                    if(!device_is_connected)
                        sdp_client_query_uuid16(&handle_sdp_client_query_result, remote_addr, BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
                    break;

                case BTSTACK_EVENT_NR_CONNECTIONS_CHANGED:
                    if (packet[2])
                    {
                        device_is_connected = 1;
                        printf("Device connected.\n");
                    }
                    else
                    {
                        device_is_connected = 0;
                        printf("Device disconnected.\n");
                        // Re-enable connection
                        sdp_client_query_uuid16(&handle_sdp_client_query_result, remote_addr, BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
                    }
                    break;

                /* LISTING_PAUSE */
                case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
                    debug("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
					break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    debug("SSP User Confirmation Request with numeric value '%" PRIu32 "'\n", little_endian_read_32(packet, 8));
                    debug("SSP User Confirmation Auto accept\n");
                    break;

                /* LISTING_RESUME */

                case L2CAP_EVENT_CHANNEL_OPENED:
                    status = packet[2];
                    if (status){
                        printf("L2CAP Connection failed: 0x%02x\n", status);
                        break;
                    }
                    l2cap_cid  = little_endian_read_16(packet, 13);
                    if (!l2cap_cid)
                    {
                        printf("No\n");
                        break;
                    }
                    if (l2cap_cid == l2cap_hid_control_cid){
                        status = l2cap_create_channel(packet_handler, remote_addr, hid_interrupt_psm, 48, &l2cap_hid_interrupt_cid);
                        if (status){
                            printf("Connecting to HID Control failed: 0x%02x\n", status);
                            break;
                        }
                        else
                            printf("HID Control connected.\n");
                    }
                    if (l2cap_cid == l2cap_hid_interrupt_cid){
                        printf("HID Connection established\n");
                    }
                    else
                        printf("HID Connection not established\n");
                    break;
                default:
                    break;
            }
            break;
        case L2CAP_DATA_PACKET:
            // for now, just dump incoming data
            printf("HID packet received: ");
            printf_hexdump(packet, size);
            if (channel == l2cap_hid_interrupt_cid){
                hid_host_handle_interrupt_report(packet,  size);
            } else if (channel == l2cap_hid_control_cid){
                debug("HID Control: ");
                debug_hexdump(packet, size);
            } else {
                break;
            }
        default:
            break;
    }
}
/* LISTING_END */

extern "C" { int btstack_main(int argc, const char * argv[]); }
int btstack_main(int argc, const char * argv[])
{
    (void)argc;
    (void)argv;

    hid_host_setup();

    // parse human readable Bluetooth address
    sscanf_bd_addr(remote_addr_string, remote_addr);

    // Turn on the device 
    hci_power_control(HCI_POWER_ON);
    return 0;
}

