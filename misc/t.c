#include<stdio.h>
#include<string.h>
#include <stdlib.h>
#include <stdbool.h>
//#include"./test.h"
typedef enum{
    CMS_FILE_HEADER = 0,
    CMS_BOUNDARY,
    CMS_DATA1,
}CMS_STATE;
typedef struct{
    int t;
    char f;
    int ts;
    int l;
    char *data_buf;
}CMS_DATA;



int main(void)
{
    //printf("打开文件失败\n");
    FILE *fp = fopen("ttt.cms","rb");
    if(fp == NULL){
        printf("打开文件失败\n");
        return 0;
       
    }
    char *boundary = NULL;
    char line_buf[1024];
    CMS_STATE state = CMS_FILE_HEADER;
    CMS_DATA *data;
    bool first = true;
    while(!feof(fp)){
        fgets(line_buf,1024,fp);
        switch(state){
        case CMS_FILE_HEADER:
            
            printf("%d\n",__LINE__);
            if(line_buf[0] == '\r' && line_buf[1] == '\n'){
                state = CMS_BOUNDARY;
            }else{
                char *p     = line_buf;
                char *key   = strsep(&p, ":");
                char *value = p;
                if (key && value) {
                    if (value[0] == ' ') { value++; };
                    if (strcmp(key, "boundary") == 0) {
                        
                        char buf[100];
                        sprintf(buf, "--%s", value);
                        boundary = strdup(buf);
                    } else if (strcmp(key, "format") == 0 && strcmp(value, "cms") == 0) {
                        //flag = flag | CMS_FLAG_FORMAT_FOUND;
                    } else if (strcmp(key, "track") == 0) {
                        //flag = flag | CMS_FLAG_TRACK_FOUND;
                        // cms->track = strdup(value);
                    }
                }
            }
            memset(line_buf,0,1024);
            break;
        case CMS_BOUNDARY:
            printf("%d\n",__LINE__);
            if(line_buf[0] == '\r' && line_buf[1] == '\n'){
            }else if(strcmp(line_buf,boundary) == 0){
                printf("%d\n",__LINE__);
                state = CMS_DATA1;
            }else{
                //出错处理
            }
            memset(line_buf,0,1024);
            break;
        case CMS_DATA1:
            printf("%d\n",__LINE__);
            if(line_buf[0] == '\r' && line_buf[1] == '\n'){
                if(data->l != 0){
                    char *data_buf = malloc(data->l);
                    int n = -1,size = data->l;
                   
                    n = fread(data_buf,1,size,fp);
                    while(n < size){
                        size -= n;
                        fseek(fp, 0, SEEK_END);
                        n =  fread(data_buf+n,1,size,fp);
                        if(n == 0){
                            if(feof(fp)){
                                break;
                            }else{
                                printf("读取数据错误\n");
                            }
                        }
                    }
                    data->data_buf = data_buf;
                    state = CMS_BOUNDARY;
                    first = true;
                }else{
                    //出错处理
                   
                }
            }else{
                if(first){
                    data = (CMS_DATA *)malloc(sizeof(CMS_DATA));
                    data->ts = 0;
                    data->l  = 0;
                    data->t  = 0;
                    first = false;
                }
               
                char *p     = line_buf;
                char *key   = strsep(&p, ":");
                char *value = p;
                if (strcmp(key, "f") == 0) {
                    if (strcmp(value, "i") == 0) {
                        data->f = 'i';
                        //data->vedio_data = 
                    } else if (strcmp(value, "p") == 0) {
                        data->f = 'p';
                    } else if (strcmp(value, "a") == 0) {
                        data->f = 'a';
                    } else if (strcmp(value, "j") == 0) {
                        data->f = 'j';
                    }
                } else if (strcmp(key, "ts") == 0) {
                    data->ts = strtol(value, NULL, 10);
                } else if (strcmp(key, "l") == 0) {
                    data->l = strtol(value, NULL, 10);
                } else if (strcmp(key, "t") == 0) {
                    data->t = strtol(value, NULL, 10);
                }
            }
            memset(line_buf,0,1024);
            break;
            
        }
        
    }
    return 0;
}
