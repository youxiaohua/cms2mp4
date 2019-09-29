#include "2MP4.h"


/* mp4文件中的box是网络字节序 写入数据的时候需要先转换字节序 */
int cms2mp4(FILE *cmsFile, char *mp4Name){
    //一级box
    BOX_FTYP ftyp;
    BOX      mdat;
    BOX      moov;
    //二级 在moov下
    BOX_MVHD mvhd;
    BOX      trak;
    //三级 在trak下
    BOX_TKHD tkhd;
    BOX      mdia;
    //四级 在mdia下
    BOX_MDHD mdhd;
    BOX_HDLR hdlr;
    BOX      minf;
    //五级 在minf下
    BOX_VMHD vmhd;
    BOX      dinf;
    BOX      stbl;
    //六级 在stbl下
    BOX_STSD stsd;
    BOX_STTS stts;
    BOX_STSS stss;
    BOX_STSC stsc;
    BOX_STSZ stsz;
    BOX_STCO stco;
    //七级 在stsd下
    BOX_AVC1 avc1;
    //八级 在avc1下
    BOX_AVCC avcC;
    

    
    
    FILE *mp4File = fopen( mp4Name, "wb" );
//写入mp4头部
    
    memset( &ftyp, 0, sizeof( ftyp ) );
    ftyp.header.boxSize = sw32( sizeof( ftyp ) );
    strncpy( (char *)&ftyp.header.boxType, BOX_TYPE_FTYP, 4 );
    FTYP_BRANDS isom = { "isom" };
    FTYP_BRANDS iso2 = { "iso2" };
    FTYP_BRANDS avc1 = { "avc1" };
    FTYP_BRANDS mp41 = { "mp41" };
    ftyp.minor_version         = sw32( 512 );
    ftyp.major_brand           = isom;
    ftyp.compatible_brands[0]  = isom;
    ftyp.compatible_brands[1]  = iso2;
    ftyp.compatible_brands[2]  = avc1;
    ftyp.compatible_brands[3]  = mp41;
    fwrite(&ftyp, sizeof(ftyp), 1, mp4File);

//写入mdat

    uint8_t *sps = NULL;
    uint8_t *pps = NULL;

    //stts entry 记录时间戳间隔和
    STTS_ENTRY *stts_entry    = (STTS_ENTRY *)calloc(1, sizeof(STTS_ENTRY));
    //stsz sample size 记录每个sample的大小
    uint32_t   *sample_sizes  = (uint32_t *)calloc(1, sizeof(uint32_t));
    //stsc entry
    STSC_ENTRY *stsc_entry    = (STSC_ENTRY *)calloc(1, sizeof(STSC_ENTRY));
    //stss sample number 记录关键帧序号
    uint32_t   *sample_number = (uint32_t *)calloc(1, sizeof(uint32_t));
    //stco chunk_offset  记录chunk的偏移量
    uint32_t   *chunk_offset  = (uint32_t *)calloc(1, sizeof(uint32_t));
    
    
//--------------------------------
    uint8_t flag             = 0;
    CMS_STATE state          = CMS_FILE_HEADER;
    CMS_DATA_TYPE data_state = UNKNOW;
    char ch;   
    while ( !feof( fp ) ) {
        ch = fgetc( fp );       
        switch ( state ) {
        case CMS_FILE_HEADER:
            if (ch == '\r') {
            } else if(ch == '\n') {
                if(line_buf_ptr == 0){
                    if (!boundary) {
                        printf("未识别到 boundary \n");
                        return -1;
                    } else if (!(flag & CMS_FLAG_FORMAT_FOUND)) {
                        printf("未识别到 format \n");
                        return -1;
                    }else if (!(flag & CMS_FLAG_TRACK_FOUND)) {
                        printf("未识别到 track \n");
                        return -1;
                    }else{
                        state = CMS_BOUNDARY;
                    }
                }else{
                    char *p     = line_buf;
                    char *key   = strsep(&p, ":");
                    char *value = p;
                    if (key && value) {
                        if ( value[0] == ' ' ) { value++; };
                        if ( strcmp(key, "boundary") == 0 ) {
                            char buf[100];
                            sprintf( buf, "\r\n--%s\r\n", value );
                            boundary = strdup( buf );
                            boundary_size = strlen( boundary );
                            boundary_ptr = 2;
                        } else if ( strcmp( key, "format" ) == 0 && strcmp(value, "cms") == 0 ) {
                            flag = flag | CMS_FLAG_FORMAT_FOUND;
                        } else if ( strcmp( key, "track" ) == 0) {
                            flag = flag | CMS_FLAG_TRACK_FOUND;
                            char *trak1 = strsep(&value, ";");
                            if(trak1 != NULL){
                                char *
                                while(1) {
                                    
                                }
                            }
                            
                        }
                    }
                    
                }
                line_buf_ptr = 0;
            }else{
                if(line_buf_ptr < sizeof(line_buf)) {
                    line_buf[line_buf_ptr] = ch;
                    line_buf_ptr++;
                    line_buf[line_buf_ptr] = 0;
                }else{
                    printf("line_buf_ptr超长\n");
                    return -1;
                }
            }
            break;
        case CMS_BOUNDARY:
            if ( ch == boundary[boundary_ptr] ) {
                boundary_ptr++;
            } else if ( boundary_ptr == boundary_size ) {
                state = CMS_DATA_HEADER;
                line_buf_ptr = 0;
                line_buf[0] = 0;
                boundary_ptr = 0;
            } else {
                if( ch == '\n' );
                {
                    
                    fseek( fp,-2,SEEK_CUR );
                    break;
                }
                printf("boundary error\n");
                return -1;
            }
            break;
        case CMS_DATA_HEADER:
            if ( ch == '\r' ) {
            } else if ( ch == '\n' ) {
                if ( line_buf_ptr == 0 ) {       
                    switch( data_state ) {
                    case CMS_VEDIO:
                        Write_Mp4(fp, &head, handle, data_length);
                        state = CMS_BOUNDARY;
                        break;
                    case CMS_AUDIO:{
                        state = CMS_BOUNDARY;
                        break;
                    }
                    default:
                        state = CMS_BOUNDARY;
                        break;
                    
                    }
                } else {               
                    char *p     = line_buf;
                    char *key   = strsep(&p, ":");
                    char *value = p;
                    if ( strcmp(key, "f" ) == 0) {
                        if (strcmp(value, "i") == 0) {
                            data_state = CMS_VEDIO;
                        } else if (strcmp(value, "p") == 0) {
                            data_state = CMS_VEDIO;
                        } else if (strcmp(value, "a") == 0) {
                            data_state = CMS_AUDIO;
                        } else if (strcmp(value, "j") == 0) {
                        
                        }
                    } else if (strcmp(key, "ts") == 0) {
                     
                    } else if (strcmp(key, "l") == 0) {
                        data_length = strtol(value, NULL, 10);
                    } else if (strcmp(key, "t") == 0) {

                    }
                }
               
                line_buf_ptr = 0;
            } else {
                if (line_buf_ptr < sizeof(line_buf)) {
                    line_buf[line_buf_ptr] = ch;
                    line_buf_ptr++;
                    line_buf[line_buf_ptr] = 0;
                } else {
                    //出错处理
                    printf("line_buf_ptr超长\n");
                    return -1;
                }
            }
            break;
            
        }
    }





    

//--------------------------------
    return 0;
}
int main(){
    cms2mp4(NULL, "123.mp4");
    int a = 1;
    printf("%d\n", ((char *)&a)[3]);
    int b = sw32(a);
    printf("%d\n", ((char *)&b)[3]);
    return 0;
}
