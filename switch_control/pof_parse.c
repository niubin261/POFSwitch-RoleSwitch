/**
 * Copyright (c) 2012, 2013, Huawei Technologies Co., Ltd.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "../include/pof_common.h"
#include "../include/pof_type.h"
#include "../include/pof_global.h"
#include "../include/pof_conn.h"
#include "../include/pof_byte_transfer.h"
#include "../include/pof_log_print.h"
#include "cjson/cJSON.h"
#include <assert.h>
#include <zconf.h>
#include <fcntl.h>

/* Xid in OpenFlow header received from Controller. */
uint32_t g_recv_xid = POF_INITIAL_XID;

static uint32_t port_pos[LEN] = {0, 1, 2, 3};

static const char *port_name[LEN] = {"v0.0", "v0.1", "v0.2", "v0.3"};

static uint32_t eth_pos[LEN] = {0, 48, 208, 240};

static uint8_t len[4] = {3, 1, 12, 16};
static char *flags[2] = {"0", "0"};//4 represent there are 4 flags can be set
static const char *eth_name[LEN] = {"eth.dst", "eth.src", "ipv4.srcAddr", "ipv4.dstAddr"};
static const char *cmd_name[7] = {"add", "edit", "edit_strict", "delete", "delete_strict", "list", "list-result"};
static const char *action_name[4] = {"drop_act;\n\t\t", "fwd_act;\n\t\t", "add_field;\n\t\t", "set_field;\n\t\t"};

static void pof_json_rule_to_nic(pof_flow_entry *flow_entry, cJSON **json, uint8_t cmd) {
    uint8_t table_id = flow_entry->table_id;
    uint16_t priority = flow_entry->priority;

    bool default_rule = false;
    char *buf_match = cJSON_PrintUnformatted(*json);
    char *buf_action = cJSON_PrintUnformatted(*(json + 1));
    char *pof_nic_cli = (char *) malloc(sizeof(char) * 2048);
    memset((void *) pof_nic_cli, 0, 2048);
    if (!buf_action && !buf_match) {
        POF_ERROR_CPRINT("the json is NULL exit the task");
        exit(EXIT_FAILURE);
    }
    if (!buf_match) {
        default_rule = true;
    }

    struct tm *now;
    time_t t;
    t = time(NULL);
    now = localtime(&t);
    char *rule_name = (char *) malloc(sizeof(char) * 64);
    memset(rule_name, 0, 64);
    sprintf(rule_name, "%d%d%d", now->tm_hour, now->tm_min, now->tm_sec);
#define NIC_CLI_PY "sudo python /home/niubin/POFSwitch-RoleSwitch/client/sdk6_rte_cli.py tables"
    pof_p4_cli_strcat(pof_nic_cli, "%s%s%s", NIC_CLI_PY, " -r ", rule_name);

    if (default_rule) {
        strcat(pof_nic_cli, " -d ");
    } else {
        pof_p4_cli_strcat(pof_nic_cli, "%s%s%s%s", " -m ", "'", buf_match, "'");
    }
    pof_p4_cli_strcat(pof_nic_cli, "%s%s%s%s", " -a ", "'", buf_action, "'");
    char table_id_ptr[8];
    sprintf(table_id_ptr, "%d", table_id);
    pof_p4_cli_strcat(pof_nic_cli, "%s%s", " -i ", table_id_ptr);
    char priority_ptr[8] = {0};
    switch (cmd) {
        case POFFC_ADD://POFFC_ADD is add rules
            sprintf(priority_ptr, "%d", priority);
            pof_p4_cli_strcat(pof_nic_cli, "%s%s%s", " -p ", priority_ptr, " add");
            break;
        case POFFC_DELETE://POFFC_DELETE is delete rules
            strcat(pof_nic_cli, " delete");
            break;
        case POFFC_MODIFY://POFFC_MODIFY is modify rules

            sprintf(priority_ptr, "%d", priority);
            pof_p4_cli_strcat(pof_nic_cli, "%s%s%s", " -p ", priority_ptr, " edit");
            break;
        default: //default is list-rules
            //Fixme TODO
            break;
    }

    POF_DEBUG_CPRINT_FL(1, GREEN, "pof_nic_cli  %s\n", pof_nic_cli);
    if (system(pof_nic_cli)) {
        POF_DEBUG("system call add_rule to smartnic\n");
    } else {
        POF_DEBUG("system call error \n");
    }

    free(buf_action);
    free(buf_match);
    pof_safe_free_mem(2, pof_nic_cli, rule_name);

    if (json) {

        cJSON_free(*(json + 1));
    }
    *(json + 1) = NULL;
}

static void pof_flow_to_nic(pof_flow_entry *flow_ptr, uint8_t cmd) {
    uint8_t i;
    uint8_t j;

    uint8_t match_field_num = flow_ptr->match_field_num;
    struct pof_match_x *match_x = flow_ptr->match;
    struct pof_match_x *match_x_tmp;
    cJSON **match_action_root = (cJSON **) malloc(sizeof(cJSON *) * 2);

    cJSON *root_match = NULL;
    root_match = cJSON_CreateObject();


    for (i = 0; i < match_field_num; i++) {
        char *k = NULL;
        char v[128] = {};
        char t[64] = {};

        match_x_tmp = (match_x + i);
        k = to_name(eth_name, eth_pos, match_x_tmp->offset);
        to_string(match_x_tmp->value, match_x_tmp->len / (uint16_t) 8, v);

        uint8_t mask_bit = 0;
        mask_bit += mask_bit_count(match_x_tmp->mask, match_x_tmp->len / (uint16_t) 8);
        sprintf(t, "%d", mask_bit);
        strcat(v, "/");
        strcat(v, t);
        cJSON *match_k;

        cJSON_AddItemToObject(root_match, k, match_k = cJSON_CreateObject());
        cJSON_AddStringToObject(match_k, "value", v);
    }

    struct pof_instruction *instructions_ptr = flow_ptr->instruction;
    uint8_t instruction_num = flow_ptr->instruction_num;
    struct pof_action *actions_ptr = NULL;
    struct pof_action *action_ptr = NULL;
    struct pof_instruction_apply_actions *pof_instruction_apply_actions_ptr = NULL;
    struct pof_instruction_goto_table *pof_instruction_goto_table_ptr = NULL;

    POF_DEBUG_CPRINT_FL(1, GREEN, "ins num = %d\n", instruction_num);
    uint32_t *port = NULL;


    uint8_t m = 0;
    for (i = 0; i < instruction_num; i++) {
        POF_DEBUG_CPRINT_FL(1, GREEN, "instruction_num : %d\n", i);
        switch (instructions_ptr[i].type) {
            case POFIT_APPLY_ACTIONS:
                pof_instruction_apply_actions_ptr = (void *) instructions_ptr->instruction_data;
                POF_DEBUG_CPRINT_FL(1, GREEN, "instruction len = %d\n", instructions_ptr[i].len);

                //POF_MAX_INSTRUCTION_LENGTH = 48*6 + 8 = 296
                actions_ptr = pof_instruction_apply_actions_ptr->action;
                for (j = 0; j < pof_instruction_apply_actions_ptr->action_num; j++) {

                    cJSON *root_action = NULL;
                    root_action = cJSON_CreateObject();
                    action_ptr = (actions_ptr + j);
                    cJSON *jfield;
                    cJSON *jfield_add_field;
                    cJSON *jport;
                    cJSON *jdata_output;
                    cJSON *jfield_del_field;

                    POF_DEBUG_CPRINT_FL(1, GREEN, "action type = %d", action_ptr->type);
                    POF_DEBUG_CPRINT_FL(1, GREEN, "action len = %d", action_ptr->len);
                    switch (action_ptr->type) {
                        case POFAT_OUTPUT: {
                            struct pof_action_output *pof_action_output_ptr = NULL;
                            pof_action_output_ptr = (void *) action_ptr->action_data;
                            port = &(pof_action_output_ptr->outputPortId);
                            POF_DEBUG_CPRINT_FL(1, GREEN, "potid = %d\n", *port);
                            POF_DEBUG_CPRINT_FL(1, GREEN, "portname = %s\n", to_name(port_name, port_pos, *(port)));

                            cJSON_AddItemToObject(root_action, "data", jdata_output = cJSON_CreateObject());
                            cJSON_AddItemToObject(jdata_output, "port", jport = cJSON_CreateObject());
                            cJSON_AddStringToObject(jport, "value", to_name(port_name, port_pos, *(port)));
                            cJSON_AddStringToObject(root_action, "type", "forward_act");
                        }

                            break;
                        case POFAT_SET_FIELD:
                            break;
                        case POFAT_ADD_FIELD: {
                            uint32_t *add_tag_len = NULL;
                            uint8_t *add_tag_value = NULL;

                            char s_field[64] = {'0', 'x'};
                            char s_field_temp[8] = {0};
                            struct pof_action_add_field *pof_action_add_field_ptr = NULL;
                            pof_action_add_field_ptr = (void *) action_ptr->action_data;
                            add_tag_value = (uint8_t *) &(pof_action_add_field_ptr->tag_value);
                            add_tag_len = &(pof_action_add_field_ptr->tag_len);

                            for (m = 0; m < *(add_tag_len) / 8; m++) {
                                sprintf(s_field_temp, "%2.2x", *(add_tag_value + (m)));
                                strcat(s_field, s_field_temp);

                            }

                            POF_DEBUG_CPRINT_FL(1, GREEN, "s_field = %s\n", s_field);
                            cJSON_AddItemToObject(root_action, "data", jfield_add_field = cJSON_CreateObject());
                            cJSON_AddItemToObject(jfield_add_field, "field", jfield = cJSON_CreateObject());
                            cJSON_AddStringToObject(jfield, "value", s_field);
                            cJSON_AddStringToObject(root_action, "type", "add_field");
                        }

                            break;
                        case POFAT_DROP:
                            cJSON_free(root_match);
                            root_match = NULL;
                            cJSON_AddStringToObject(root_action, "type", "drop_act");
                            break;
                        case POFAT_DELETE_FIELD: {
                            struct pof_action_delete_field *pof_action_del_field_ptr = NULL;
                            (pof_action_del_field_ptr) = (void *) action_ptr->action_data;
                            cJSON_AddItemToObject(root_action, "data", jfield_del_field = cJSON_CreateObject());
                            cJSON_AddItemToObject(jfield_del_field, "field", jfield = cJSON_CreateObject());
                            cJSON_AddStringToObject(root_action, "type", "del_field");
                        }

                            break;
                        case POFAT_CALCULATE_CHECKSUM:

                            break;
                        case POFAT_MODIFY_FIELD:
                            break;
                        case POFAT_PACKET_IN:
                            //TODO do not used in nic
                        default:
                            break;

                    }
                    if (root_action && root_match) {
                        POF_DEBUG_CPRINT_FL(1, GREEN, "match action json = \n %s + %s\n",
                                            cJSON_Print(root_match), cJSON_Print(root_action));
                    }
                    *match_action_root = root_match;
                    *(match_action_root + 1) = root_action;
                    pof_json_rule_to_nic(flow_ptr, match_action_root, cmd);

                }
                break;
            case POFIT_WRITE_ACTIONS:
                //TODO do not used in POF
                break;
            case POFIT_CLEAR_ACTIONS:
                //TODO do not used in POF
                break;
            case POFIT_GOTO_DIRECT_TABLE:

                break;
            case POFIT_GOTO_TABLE: {
                pof_instruction_goto_table_ptr = (void *) instructions_ptr->instruction_data;

                cJSON *root_action = NULL;
                root_action = cJSON_CreateObject();
                uint8_t next_table_id = 0;

                next_table_id = pof_instruction_goto_table_ptr->next_table_id;
                cJSON *jnext_table_id[4] = {};

                for (uint8_t flag_id = 0; flag_id < 2; flag_id++) {

                    cJSON_AddItemToObject(root_action, "data", jnext_table_id[flag_id] = cJSON_CreateObject());
                    if (flag_id == next_table_id) {
                        flags[flag_id] = "1";
                    }
                    cJSON_AddStringToObject(jnext_table_id[flag_id], "value", flags[flag_id]);
                }

                cJSON_AddStringToObject(root_action, "type", "modify_flag");
                if (root_action && root_match) {
                    POF_DEBUG_CPRINT_FL(1, GREEN, "match action json = \n %s + %s\n",
                                        cJSON_Print(root_match), cJSON_Print(root_action));
                }
                *match_action_root = root_match;
                *(match_action_root + 1) = root_action;
                pof_json_rule_to_nic(flow_ptr, match_action_root, cmd);
            }

                break;
            case POFIT_METER:
                break;
            case POFIT_WRITE_METADATA:
                break;
            case POFIT_WRITE_METADATA_FROM_PACKET:
                break;

            default:
                break;
        }


    }
    if (match_action_root) {
        cJSON_free(*(match_action_root));
        free(match_action_root);
    }

    return;
}


static void pof_send_msg_to_nic(char *msg_ptr) {
    pof_flow_entry *flow_ptr;
    flow_ptr = (pof_flow_entry *) (msg_ptr + sizeof(pof_header));
    struct nic_match *nic_match_ptr = NULL;
    if (flow_ptr->command == POFFC_ADD) {
        pof_flow_to_nic(flow_ptr, POFFC_ADD);
    } else if (flow_ptr->command == POFFC_DELETE) {
        POF_DEBUG_CPRINT_FL(1, GREEN, "flow command is %d ", flow_ptr->command);
        pof_flow_to_nic(flow_ptr, POFFC_DELETE);
    } else if (flow_ptr->command == POFFC_MODIFY) {
        pof_flow_to_nic(flow_ptr, POFFC_MODIFY);
    } else {
    }

}

#define MAX_LINE_LENGTH 1024


static void pof_create_table(char *table_name, uint8_t table_type,
                      char **name, uint8_t names_size,
                      bool ingress, bool egress,
                      uint8_t actions, uint8_t valid) {
    FILE *p4 = fopen("p4", "r+");
    if (p4 == NULL) {
        POF_ERROR_CPRINT(1, RED, "p4 file open failed");
        return;
    }
    char *p4_contents = (char *) malloc(sizeof(char) * 100 * MAX_LINE_LENGTH);
    memset(p4_contents, 0, MAX_LINE_LENGTH * 100);
    fseek(p4, 0, SEEK_END);
    uint64_t size = ftell(p4);
    rewind(p4);
    assert(fread(p4_contents, 1, size, p4) != 0);

    char *insert_line_pos = strstr(p4_contents, "control");
    if (insert_line_pos == NULL) {
        return;
    }
    char *buf_tmp = (char *) malloc(sizeof(char) * MAX_LINE_LENGTH * 20);
    memset(buf_tmp, 0, MAX_LINE_LENGTH * 10);
    strcpy(buf_tmp, insert_line_pos);

    char *table = (char *) malloc(sizeof(char) * MAX_LINE_LENGTH * 10);
    memset(table, 0, MAX_LINE_LENGTH * 10);
    sprintf(table, "%stable %s{\n\treads{\n\t\t", table, table_name);
    uint8_t i = 0;
    for (; i < names_size; i++) {
        sprintf(table, "%s%s%s", table, name[i], ":");
        switch (table_type) {
            case POF_MM_TABLE:
                strcat(table, "lpm");
                break;
            case POF_LPM_TABLE:
                strcat(table, "lpm");
                break;
            case POF_EM_TABLE:
                strcat(table, "exact");
                break;
            default:
                break;
        }
        strcat(table, ";\n\t\t");
    }
    table[strlen(table) - 1] = '\0';
    sprintf(table, "%s}\n\tactions{\n\t\t", table);
    pof_add_action_to_p4(action_name, actions, table);

    table[strlen(table) - 1] = '\0';
    strcat(table, "}\n}\n");
    POF_DEBUG_CPRINT_FL(1, GREEN, "p4 table is %s \n", table);
    strcpy(insert_line_pos, table);
    char *ingress_control = NULL;
    char *egress_control = NULL;
    ingress_control = match_ingress_control(buf_tmp);
    egress_control = match_egress_control(buf_tmp);
    if (ingress_control == NULL) {
        POF_DEBUG_CPRINT(1, GREEN, "the ingress is NULL");
        return;
    }
#define POF_VALID_FLAG(x) "my_metadata.field"#x
    char *buf_tmp_control = (char *) malloc(sizeof(char) * MAX_LINE_LENGTH * 10);

    POF_DEBUG_CPRINT_FL(1, GREEN, "the ingress/egress control is %s/%s", ingress_control, egress_control);
    if (ingress) {
        if (valid) {
            strcpy(buf_tmp_control, ingress_control);
            memset(ingress_control, 0, strlen(ingress_control));
            sprintf(ingress_control, "%s%s%s%s", ingress_control, "\tif(", POF_VALID_FLAG(_1), "){\n\t\t");
            sprintf(ingress_control, "%sapply"
                    "(%s);\n\t}\n", ingress_control, table_name);
            strcat(buf_tmp, buf_tmp_control);

        } else {
            strcpy(buf_tmp_control, ingress_control);
            strcpy(ingress_control, "apply(");
            sprintf(ingress_control, "%s%s);\n", ingress_control, table_name);
            strcat(buf_tmp, buf_tmp_control);
        }

    } else {
        if (valid) {
            strcpy(buf_tmp_control, egress_control);
            memset(egress_control, 0, strlen(egress_control));
            sprintf(egress_control, "%s%s%s%s", egress_control, "\tif(", POF_VALID_FLAG(_1), "){\n\t\t");
            sprintf(egress_control, "%sapply"
                    "(%s);\n\t}\n", egress_control, table_name);
            strcat(buf_tmp, buf_tmp_control);
        } else {
            strcpy(buf_tmp_control, egress_control);
            strcpy(egress_control, "apply(");
            sprintf(egress_control, "%s%s);\n", egress_control, table_name);
            strcat(buf_tmp, buf_tmp_control);

        }
    }

    strcat(p4_contents, buf_tmp);
    POF_DEBUG_CPRINT_FL(1, GREEN, "p4 is %s \n", p4_contents);
    rewind(p4);
    assert(fwrite(p4_contents, 1, strlen(p4_contents), p4) != 0);
    pof_safe_free_mem(4, table, buf_tmp, buf_tmp_control, p4_contents);
    insert_line_pos = NULL;
    fflush(p4);
    fclose(p4);
    return;
}

static void pof_p4_compile() {

    FILE *p4 = fopen("p4", "r");
    if (p4 == NULL) {
        POF_ERROR_CPRINT(1, GREEN, "")
    }
    //p4 front-end compiler

#define P4_FRONT_END_COMPILER "sudo /opt/netronome/p4/bin/nfp4c -D NO_MULTICAST -o /tmp/p4.yml --source_info p4"
    if (system(P4_FRONT_END_COMPILER) != 0) {
        POF_ERROR_CPRINT(1, GREEN, "p4 front-end compiler failed");
        fclose(p4);
        return;
    } else {
        POF_DEBUG_CPRINT(1, GREEN, "p4 front-end compiler successfully");
    }
    //p4 back-end compiler
#define P4_BACK_END_COMPILER "sudo /opt/netronome/p4/bin/nfirc -o /tmp /tmp/p4.yml"
    if (system(P4_BACK_END_COMPILER) != 0) {
        fclose(p4);
        POF_ERROR_CPRINT(1, GREEN, "p4 back-end compiler failed");
        return;
    } else {
        POF_DEBUG_CPRINT(1, GREEN, "p4 back-end compiler successfully");
    }
    //building p4 applications
#define P4_APP_BUILDING "sudo /opt/netronome/p4/bin/nfp4build -o /tmp/p4.nffw -l hydrogen -4 p4"
    if (system(P4_APP_BUILDING) != 0) {
        POF_ERROR_CPRINT(1, GREEN, "p4 app build failed");
        fclose(p4);
        return;
    } else {
        POF_DEBUG_CPRINT(1, GREEN, "p4 app build successfully");
    }

    //load .nffw into nic using runtime api
#define P4_PY_LOAD_NFFW "sudo /opt/netronome/p4/bin/rtecli design-load -f /tmp/p4.nffw -p /tmp/pif_design.json "
    if (system(P4_PY_LOAD_NFFW) != 0) {
        POF_ERROR_CPRINT(1, GREEN, "p4 load fireware failed");
        fclose(p4);
        return;
    } else {
        POF_DEBUG_CPRINT(1, GREEN, "p4 load fireware successfully");
    }
    fclose(p4);
    return;
}

static void pof_table_to_p4(void *msg_ptr) {

    POF_DEBUG_CPRINT(1, GREEN, "pof_table_to_p4");
    struct pof_flow_table *table_ptr = (struct pof_flow_table *) (msg_ptr + sizeof(pof_header));
    uint8_t i = 0;
    bool ingress = false;
    bool egress = false;
    struct pof_match *pof_match_ptr;
    uint16_t offset = 0;
    uint16_t len = 0;
    char *table_name = NULL;
    uint8_t table_type = table_ptr->type;
    char **names = (char **) malloc(sizeof(char *) * table_ptr->match_field_num);
    table_name = table_ptr->table_name;

    for (i = 0; i < table_ptr->match_field_num; i++) {
        pof_match_ptr = (table_ptr->match + i);
        offset = pof_match_ptr->offset;
        len = pof_match_ptr->len;
        POF_DEBUG_CPRINT_FL(1, GREEN, "match %d offset is %d len is %d\n", i, offset, len);
        names[i] = to_name(eth_name, eth_pos, offset);

    }
    ingress = *(table_ptr->pad);
    uint8_t actions = *(table_ptr->pad + 1);
    uint8_t valid = *(table_ptr->pad + 2);
    pof_create_table(table_name, table_type, names, table_ptr->match_field_num, ingress, egress, actions, valid);
    free(names);
    pid_t pid;
    pid = fork();
    if (pid == 0) {
        int fd = open("out", O_RDWR, 0);
        if (fd == -1) {
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDERR_FILENO);
        //pof_p4_compile();
        dup2(STDERR_FILENO, fd);
        close(fd);
        exit(EXIT_SUCCESS);
    }
    return;
}

/*******************************************************************************
 * Parse the OpenFlow message received from the Controller.
 * Form:     uint32_t  pof_parse_msg_from_controller(char* msg_ptr)
 * Input:    message length, message data
 * Output:   NONE
 * Return:   POF_OK or Error code
 * Discribe: This function parses the OpenFlow message received from the Controller,
 *           and execute the response.
*******************************************************************************/
uint32_t pof_parse_msg_from_controller(char *msg_ptr, struct pof_datapath *dp, int i) {
    struct pof_local_resource *lr, *next;
    uint16_t slot = POF_SLOT_ID_BASE;
    pof_switch_config *config_ptr;
    pof_header *header_ptr, head;
    pof_flow_entry *flow_ptr;
    pof_role_request *role_ptr;
    pof_role_reply role_reply;
    pof_packet_out *packet_out;//add by wenjian 2015/12/01
    pof_counter *counter_ptr;
    pof_flow_table *table_ptr;
    pof_port *port_ptr;
    pof_meter *meter_ptr;
    pof_group *group_ptr;
    struct pof_queryall_request *queryall_ptr;
    struct pof_slot_config *slotConfig;
    struct pof_instruction_block *pof_insBlock;
    uint32_t ret = POF_OK;
    uint16_t len;
    uint8_t msg_type;
    //uint32_t          queue_id =pofsc_send_q_id[i];
    uint8_t j, role;
    header_ptr = (pof_header *) msg_ptr;
    len = POF_NTOHS(header_ptr->length);

    /* Print the parse information. */
#ifndef POF_DEBUG_PRINT_ECHO_ON
    if (header_ptr->type != POFT_ECHO_REPLY) {
#endif
        POF_DEBUG_CPRINT_PACKET(msg_ptr, 0, len);
#ifndef POF_DEBUG_PRINT_ECHO_ON
    }
#endif

    /* Parse the OpenFlow packet header. */
    pof_NtoH_transfer_header(header_ptr);
    msg_type = header_ptr->type;
    g_recv_xid = header_ptr->xid;

    /* Execute different responses according to the OpenFlow type. */
    switch (msg_type) {
        case POFT_ECHO_REQUEST:
            if (POF_OK != pofec_reply_msg(i, POFT_ECHO_REPLY, g_recv_xid, 0, NULL)) {
                POF_ERROR_HANDLE_RETURN_UPWARD(POFET_SOFTWARE_FAILED, POF_WRITE_MSG_QUEUE_FAILURE, g_recv_xid, i);
            }
            break;

        case POFT_SET_CONFIG:
            config_ptr = (pof_switch_config *) (msg_ptr + sizeof(pof_header));
            pof_HtoN_transfer_switch_config(config_ptr);

            ret = poflr_set_config(config_ptr->flags, config_ptr->miss_send_len);
            POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            break;

        case POFT_GET_CONFIG_REQUEST:
        POF_DEBUG_CPRINT_FL(1, GREEN, "@POFT_GET_CONFIG_REQUEST\n");
            ret = poflr_reply_config(i);
            POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);

            HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                ret = poflr_reply_table_resource(i, lr);
                POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            }
            HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                ret = poflr_reply_port_resource(i, lr);
                POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            }
#ifdef POF_SHT_VXLAN
        HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap){
            ret = poflr_reply_slot_status(i,lr, POFSS_UP, POFSRF_RE_SEND);
            POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
        }
#endif // POF_SHT_VXLAN
            break;

        case POFT_PORT_MOD:
            port_ptr = (pof_port *) (msg_ptr + sizeof(pof_header) + sizeof(pof_port_status) - sizeof(pof_port));
            pof_NtoH_transfer_port(port_ptr);

#ifdef POF_MULTIPLE_SLOTS
            slot = port_ptr->slotID;
#endif // POF_MULTIPLE_SLOTS
            if ((lr = pofdp_get_local_resource(slot, dp)) == NULL) {
                POF_ERROR_HANDLE_RETURN_UPWARD(POFET_SOFTWARE_FAILED, POF_INVALID_SLOT_ID, g_recv_xid, i);
            }

            ret = poflr_port_openflow_enable(port_ptr->port_id, port_ptr->of_enable, lr);
            POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            break;

#ifdef POF_SHT_VXLAN
        case POFT_SLOT_CONFIG:
            slotConfig = (struct pof_slot_config *)(msg_ptr + sizeof(pof_header));
            pof_HtoN_transfer_slot_config(slotConfig);

            if((lr = pofdp_get_local_resource(slotConfig->slotID, dp)) == NULL){
                POF_ERROR_HANDLE_RETURN_UPWARD(POFET_SOFTWARE_FAILED, POF_INVALID_SLOT_ID, g_recv_xid,i);
            }

            struct portInfo *port, *nextPort;
            /* Traverse all ports. */
            HMAP_NODES_IN_STRUCT_TRAVERSE(port, nextPort, pofIndexNode, lr->portPofIndexMap){
                ret = poflr_port_openflow_enable(port->pofIndex, slotConfig->flag, lr);
                POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            }

            break;

        case POFT_INSTRUCTION_BLOCK_MOD:
            pof_insBlock = (struct pof_instruction_block *)(msg_ptr + sizeof(pof_header));
            pof_NtoH_transfer_insBlock(pof_insBlock);

            if(pof_insBlock->command == POFFC_ADD){
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap){
                    ret = poflr_add_insBlock(pof_insBlock, lr);
                }
            }else if(pof_insBlock->command == POFFC_MODIFY){
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap){
                    ret = poflr_modify_insBlock(pof_insBlock, lr);
                }
            }else if(pof_insBlock->command == POFFC_DELETE){
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap){
                    ret = poflr_delete_insBlock(pof_insBlock->instruction_block_id, lr);
                }
            }else{
                POF_ERROR_HANDLE_RETURN_UPWARD(POFET_INSBLOCK_MOD_FAILED, POFIMFC_BAD_COMMAND, g_recv_xid,i);
            }

            POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            break;
#endif // POF_SHT_VXLAN

        case POFT_FEATURES_REQUEST:
            ret = poflr_reset_dev_id();
            POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            POF_DEBUG_CPRINT_FL(1, GREEN, ">>Handle features request message");
            HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                ret = poflr_reply_feature_resource(i, lr);
                POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            }
            break;

        case POFT_TABLE_MOD:
            table_ptr = (pof_flow_table *) (msg_ptr + sizeof(pof_header));
            pof_NtoH_transfer_flow_table(table_ptr);

            if (pofsc_conn_desc[i].role != ROLE_MASTER) break;
            if (table_ptr->command == POFTC_ADD) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    ret = poflr_create_flow_table(i, \
                                                  table_ptr->tid, \
                                                  table_ptr->type, \
                                                  table_ptr->key_len, \
                                                  table_ptr->size, \
                                                  table_ptr->table_name, \
                                                  table_ptr->match_field_num, \
                                                  table_ptr->match, \
                                                  lr);
                }
                pof_table_to_p4(msg_ptr);
            } else if (table_ptr->command == POFTC_DELETE) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    ret = poflr_delete_flow_table(i, table_ptr->tid, table_ptr->type, lr);
                }
            } else {
                POF_ERROR_HANDLE_RETURN_UPWARD(POFET_TABLE_MOD_FAILED, POFTMFC_BAD_COMMAND, g_recv_xid, i);
            }


            POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            break;

        case POFT_FLOW_MOD:
            flow_ptr = (pof_flow_entry *) (msg_ptr + sizeof(pof_header));
            pof_NtoH_transfer_flow_entry(flow_ptr);
            if (pofsc_conn_desc[i].role != ROLE_MASTER) break;
            if (flow_ptr->command == POFFC_ADD) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {

                    ret = poflr_add_flow_entry(flow_ptr, lr, i);

                }
            } else if (flow_ptr->command == POFFC_DELETE) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    ret = poflr_delete_flow_entry(flow_ptr, lr, i);
                }
            } else if (flow_ptr->command == POFFC_MODIFY) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    ret = poflr_modify_flow_entry(flow_ptr, lr, i);
                }
            } else {
                POF_ERROR_HANDLE_RETURN_UPWARD(POFET_FLOW_MOD_FAILED, POFFMFC_BAD_COMMAND, g_recv_xid, i);
            }
            POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
//            usr_cmd_tables();
            pof_send_msg_to_nic(msg_ptr);
            break;


        case POFT_ROLE_REQUEST:
            role_ptr = (pof_role_request *) (msg_ptr + sizeof(pof_header));

            role_reply.role = role_ptr->role;

            if (role_ptr->role == ROLE_MASTER) {
                for (j = 0; j < n_controller; j++) {
                    if (pofsc_conn_desc[j].role == ROLE_MASTER) {
                        pofsc_conn_desc[j].role = ROLE_SLAVE;
                    }
                }
                pofsc_conn_desc[i].role = ROLE_MASTER;
                master_controller = i;
                if (POF_OK !=
                    pofec_reply_msg(i, POFT_ROLE_REPLY, g_recv_xid, sizeof(pof_role_reply), (uint8_t *) &role_reply)) {
                    POF_ERROR_HANDLE_RETURN_UPWARD(POFET_ROLE_REQUEST_FAILED, POF_WRITE_MSG_QUEUE_FAILURE, g_recv_xid,
                                                   i);
                }

            } else if (role_ptr->role == ROLE_EQUAL) {
                POF_DEBUG_CPRINT(1, GREEN, ">>\nThis request role is standby");
                if (pofsc_conn_desc[i].role == ROLE_MASTER) { master_controller = -1; }
                pofsc_conn_desc[i].role = ROLE_EQUAL;
                if (POF_OK !=
                    pofec_reply_msg(i, POFT_ROLE_REPLY, g_recv_xid, sizeof(pof_role_reply), (uint8_t *) &role_reply)) {
                    POF_ERROR_HANDLE_RETURN_UPWARD(POFET_ROLE_REQUEST_FAILED, POF_WRITE_MSG_QUEUE_FAILURE, g_recv_xid,
                                                   i);
                }

            }

            break;


            /*add by wenjian 2015/12/01*/
        case POFT_PACKET_OUT:
            //first move the pointer to the packet_out from header
            if (pofsc_conn_desc[i].role != ROLE_MASTER) break;
            packet_out = (pof_packet_out *) (msg_ptr + sizeof(pof_header));
            //take transfer from n to h
            pof_NtoH_transfer_packet_out(packet_out);
            //POF_DEBUG_CPRINT_OX_NO_ENTER(packet_out,sizeof(packet_out));
            struct pofdp_packet dpp[1] = {0};
            //POF_DEBUG_CPRINT(1,BLUE,"===============start memset\n");
            memset(dpp, 0, sizeof *dpp);
            //POF_DEBUG_CPRINT(1,BLUE,"===============memset success\n");
            //apply the packet_out to the pofdp_packet
            dpp->packetBuf = dpp->buf;
            //POF_DEBUG_CPRINT(1,BLUE,"===============%d point packetbuf to memory success\n",packet_out->packetLen);
            memcpy(dpp->buf, packet_out->data, packet_out->packetLen);
            //POF_DEBUG_CPRINT(1,BLUE,"===============memcpy success\n");
            /* Store packet data, length, received port infomation into the message queue. */
            // dpp->output_port_id=packet_out->inPort;
            dpp->ori_port_id = packet_out->inPort;
            dpp->ori_len = packet_out->packetLen;
            dpp->left_len = dpp->ori_len;
            dpp->buf_offset = dpp->packetBuf;
            dpp->dp = dp;
            dpp->act = (pof_action *) (packet_out->actionList);
            POF_DEBUG_CPRINT_0X_NO_ENTER(dpp->act, 48 * 6);
            dpp->act_num = packet_out->actionNum;
            //POF_DEBUG_CPRINT(1,BLUE,"===============dpp initialize success\n");
            ret = pofdp_action_execute(dpp, lr);
            //POF_DEBUG_CPRINT(1,BLUE,"===============excute actions success\n");
            POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            //free(dp);
            break;


        case POFT_METER_MOD:
            meter_ptr = (pof_meter *) (msg_ptr + sizeof(pof_header));
            pof_NtoH_transfer_meter(meter_ptr);

            if (meter_ptr->command == POFMC_ADD) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    ret = poflr_add_meter_entry(meter_ptr->meter_id, meter_ptr->rate, lr);
                }
            } else if (meter_ptr->command == POFMC_MODIFY) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    ret = poflr_modify_meter_entry(meter_ptr->meter_id, meter_ptr->rate, lr);
                }
            } else if (meter_ptr->command == POFMC_DELETE) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    ret = poflr_delete_meter_entry(meter_ptr->meter_id, meter_ptr->rate, lr);
                }
            } else {
                POF_ERROR_HANDLE_RETURN_UPWARD(POFET_METER_MOD_FAILED, POFMMFC_BAD_COMMAND, g_recv_xid, i);
            }

            POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            break;

        case POFT_GROUP_MOD:
            group_ptr = (pof_group *) (msg_ptr + sizeof(pof_header));
            pof_NtoH_transfer_group(group_ptr);

            if (group_ptr->command == POFGC_ADD) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    ret = poflr_add_group_entry(group_ptr, lr);
                }
            } else if (group_ptr->command == POFGC_MODIFY) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    ret = poflr_modify_group_entry(group_ptr, lr);
                }
            } else if (group_ptr->command == POFGC_DELETE) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    ret = poflr_delete_group_entry(group_ptr, lr);
                }
            } else {
                POF_ERROR_HANDLE_RETURN_UPWARD(POFET_GROUP_MOD_FAILED, POFGMFC_BAD_COMMAND, g_recv_xid, i);
            }

            POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            break;

        case POFT_COUNTER_MOD:
            counter_ptr = (pof_counter *) (msg_ptr + sizeof(pof_header));
            pof_NtoH_transfer_counter(counter_ptr);

            if (counter_ptr->command == POFCC_CLEAR) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    ret = poflr_counter_clear(counter_ptr->counter_id, lr);
                }
            } else if (counter_ptr->command == POFCC_ADD) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    ret = poflr_counter_init(counter_ptr->counter_id, lr);
                }
            } else if (counter_ptr->command == POFCC_DELETE) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    ret = poflr_counter_delete(counter_ptr->counter_id, lr);
                }
            } else {
                POF_ERROR_HANDLE_RETURN_UPWARD(POFET_COUNTER_MOD_FAILED, POFCMFC_BAD_COMMAND, g_recv_xid, i);
            }

            POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            break;

        case POFT_COUNTER_REQUEST:
            counter_ptr = (pof_counter *) (msg_ptr + sizeof(pof_header));
            pof_NtoH_transfer_counter(counter_ptr);

            HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                ret = poflr_get_counter_value(counter_ptr->counter_id, lr, i);
                POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            }
            break;

        case POFT_QUERYALL_REQUEST:
            queryall_ptr = (struct pof_queryall_request *) (msg_ptr + sizeof(pof_header));
            pof_HtoN_transfer_queryall_request(queryall_ptr);

            if (queryall_ptr->slotID == POFSID_ALL) {
                HMAP_NODES_IN_STRUCT_TRAVERSE(lr, next, slotNode, dp->slotMap) {
                    /* Query all resource on all slots. TODO */
                    ret = poflr_reply_queryall(lr, i);
                    POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
                }
            } else {
                if ((lr = pofdp_get_local_resource(queryall_ptr->slotID, dp)) == NULL) {
                    POF_ERROR_HANDLE_RETURN_UPWARD(POFET_SOFTWARE_FAILED, POF_INVALID_SLOT_ID, g_recv_xid, i);
                }
                /* Query all resource on one slots. TODO */
                ret = poflr_reply_queryall(lr, i);
                POF_CHECK_RETVALUE_RETURN_NO_UPWARD(ret);
            }

            if (POF_OK != pofec_reply_msg(i, POFT_QUERYALL_FIN, g_recv_xid, 0, NULL)) {
                POF_ERROR_HANDLE_RETURN_UPWARD(POFET_SOFTWARE_FAILED, POF_WRITE_MSG_QUEUE_FAILURE, g_recv_xid, i);
            }
            break;

        default:
        POF_ERROR_HANDLE_RETURN_UPWARD(POFET_BAD_REQUEST, POFBRC_BAD_TYPE, g_recv_xid, i);
            break;
    }
    return ret;
}
