//
// Created by niubin on 18-3-22.
//

#include "cjson/cJSON.h"
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <math.h>
void to_string(unsigned * arr, unsigned len,char* str){
//    str = (char*)malloc(sizeof(char)*(len + 1));
//    unsigned i = 0;
//    for (;i < len; i++) {
//        str[i] = arr[i] + '0';
//        printf("str[%d] = %c\n",i,str[i]);
//        str[i + 1] = '.';
//        printf("str[%d] = %c\n",i+1,str[i+1]);
//
//    }
//    str[len] = '\0';
//    printf("str = %s",str);
//    return str;
    char str1[4];
    char tmp[32];
    int i = 0;
    for (;i < len - 1; i++) {
        //itoa(arr[i],tmp,10);
        sprintf(tmp,"%d",arr[i]);
        strcat(str1,tmp);
        strcat(str1,".");
        printf("str = %s\n",str1);

    }
    printf("str = %s\n",str1);
    sprintf(tmp,"%d",arr[len - 1]);
    //itoa(arr[len - 1],tmp,10);
    strcat(str1,tmp);
    printf("str = %s\n",str1);

}
#define POF_MOVE_BIT_LEFT(x,n)              ((x) << (n))
#define POF_MOVE_BIT_RIGHT(x,n)              ((x) >> (n))
typedef __uint32_t uint32_t;
typedef  __uint8_t uint8_t;
typedef __uint16_t uint16_t;
void pofbf_cover_bit(uint8_t *data_ori, const uint8_t *value, uint16_t pos_b, uint16_t len_b){
    uint32_t process_len_b = 0;
    uint16_t pos_b_x, len_B, after_len_b_x;
    uint8_t *ptr;

    pos_b_x = pos_b % 8;
    len_B = (uint16_t)((len_b - 1) / 8 + 1);
    after_len_b_x = (len_b + pos_b - 1) % 8 + 1;
    ptr = data_ori + (uint16_t)(pos_b / 8);

    if(len_b <= (8 - pos_b_x)){
        *ptr = (*ptr & POF_MOVE_BIT_LEFT(0xff, (8-pos_b_x)) \
            | (POF_MOVE_BIT_RIGHT(*value, pos_b_x) & POF_MOVE_BIT_LEFT(0xff, 8-after_len_b_x)) \
            | (*ptr & POF_MOVE_BIT_RIGHT(0xff, after_len_b_x)));
        return;
    }

    *ptr = *ptr & POF_MOVE_BIT_LEFT(0xff, (8-pos_b_x)) \
        | POF_MOVE_BIT_RIGHT(*value, pos_b_x);

    process_len_b = 8 - pos_b_x;
    while(process_len_b < (len_b-8)){
        *(++ptr) = POF_MOVE_BIT_LEFT(*value, 8-pos_b_x) | POF_MOVE_BIT_RIGHT(*(++value), pos_b_x);
        process_len_b += 8;
    }

    *(ptr+1) = (POF_MOVE_BIT_LEFT(*value, 8-pos_b_x) | POF_MOVE_BIT_RIGHT(*(++value), pos_b_x) \
        & POF_MOVE_BIT_LEFT(0xff, 8 - (len_b - process_len_b))) \
        | (*(ptr+1) & POF_MOVE_BIT_RIGHT(0xff, len_b - process_len_b));

    return;
}

uint8_t bits_to_uint8_t(uint8_t*result_cnt,uint8_t pos,uint8_t len){
    uint8_t result_uint8_t = 0;
    uint8_t i = 0;
    for (;i < len; i++){
        result_uint8_t +=  pow(2,*(result_cnt + pos + len));
    }
    return result_uint8_t;
}
int main(char argc,char**argv){
//    ushort action_num = 1;
//
//    cJSON* root;
//    root = cJSON_CreateObject();
//    cJSON* second_root;
//    cJSON_AddItemToObject(root,"fwd",second_root = cJSON_CreateObject());
//    cJSON* jlist;
//    cJSON_AddItemToObject(second_root,"rule",jlist = cJSON_CreateArray());
//    cJSON* third_root;
//    cJSON_AddItemToArray(jlist,third_root = cJSON_CreateObject());
//    cJSON* matchs_jlist;
//    cJSON_AddItemToObject(third_root,"matchs",matchs_jlist = cJSON_CreateArray());
//    cJSON* match;
//    cJSON_AddItemToArray(matchs_jlist,match = cJSON_CreateObject());
//    cJSON_AddStringToObject(match,"ipsrc","10.0.0.0/16");
//    cJSON_AddStringToObject(match,"ipdst","10.0.0.1/16");
//    cJSON* actions_jlist;
//    cJSON_AddItemToObject(third_root,"actions",actions_jlist = cJSON_CreateArray() );
//    for (int i = 0; i < action_num; i++){
//
//    }
//    cJSON* action;
//    cJSON_AddItemToArray(actions_jlist,action = cJSON_CreateObject());
//    cJSON_AddStringToObject(action,"output","vf0_2");
//
////        match_offset[i] = match_x_tmp->offset;
////        match_len[i] = match_x_tmp->len;
////        match[i] = (uint8_t*)malloc(sizeof(uint8_t)*match_x_tmp->len/8);
////        mask[i] = (uint8_t*)malloc(sizeof(uint8_t)*match_x_tmp->len/8);
//    //POF_DEBUG_CPRINT_FL(1,BLUE,"-------match val = ")
//    //POF_DEBUG_CPRINT_FL(1,BLUE, "-------match val = %u-------\n",match_x_tmp->value);
//    //match[i][j] = match_x_tmp->value[j];
//
//
//
//
//    char *s = cJSON_PrintUnformatted(root);
//    if (s) {
//        printf("%s\n",s);
//        free(s);
//    }
//    if (root)
//        cJSON_Delete(root);
//    int*p = malloc(4);
//    free(p);
//    unsigned arr[6] = {10,0,0,1,1,2};
//    char*str = NULL;
//    to_string(arr,6,str);
//    printf("str = %s\n",str);
//    return 0;

    typedef unsigned char uint8_t;
    typedef unsigned short uint16_t;
    typedef unsigned int uint32_t;
    typedef unsigned long int uint64_t;
    uint32_t a = 0x00018100;
    uint8_t pos = 27;
    uint8_t len = 12;
    uint8_t i = 0;
    uint32_t mask = 0;
    uint8_t tmp = 0;
//    for (;i < len; i++) {
//        mask |= (1 << (pos - i));
//    }
//    uint32_t result =( a & mask ) >> (pos - len + 1);
 //   printf("%d\n",result);
    uint8_t ret[64] = {0};
    uint8_t val = 129;

    for (i = 0;i < 64; i++){
       // printf("%d\n",ret[i]);
    }
    uint8_t* result[4] = {0,1,129,0};
    printf("%d\n",bits_to_uint8_t(result,4,12));
    uint64_t h = 0x00;
    printf("h = %.2x ",h);

}