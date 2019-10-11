#include "2MP4.h"



//读取到track信息时 获取视频的信息
int get_information(char *value, BOX_TKHD *tkhd){
   
    char *trak = value;
    char *trak1  = strsep(&trak, ";");
    if(trak1 != NULL) {
        
        
        trak1 = strtok(trak1,",");
        
        while(trak1 != NULL) {
            if(strstr(trak1, "width") != NULL) {
                tkhd->width = sw32( (uint32_t)atoi( &trak1[6] ) );
                printf("width : %d\n", atoi( &trak1[6] ));
            }
            if(strstr(trak1, "height") != NULL) {
                tkhd->height = sw32( (uint32_t)atoi( &trak1[7] ) );
                printf("height : %d\n", atoi( &trak1[7] ));
            }
            trak1 = strtok(NULL, ",");
        }
    }
}
uint32_t add_sample_size(uint32_t sample_count, uint32_t **sample_sizes, int DataSize) {
    if(sample_count == 0) {
        (*sample_sizes)[0] = sw32(DataSize);
        return sw32(1);
    }
    sample_count = sw32(sample_count) + 1;
    *sample_sizes = (uint32_t *)realloc(*sample_sizes, sizeof(uint32_t) * sample_count);
    (*sample_sizes)[sample_count - 1] = sw32(DataSize);
    return sw32(sample_count);
}

//读取到时间间隔时写入 stts中
void add_delta( uint32_t delta, STTS_ENTRY **stts_entry, BOX_STTS *stts ) {
    if( delta == 0 ){ return; }
    delta = sw32( delta );
    stts->entry_count = sw32( stts->entry_count ); //先转换成小端序  用完再转回大端序
    if( stts->entry_count == 0 ) {
        (*stts_entry)->sample_count = sw32( 1 );
        (*stts_entry)->sample_delta = sw32(delta);
        return;
    }
    int i, find = 0;
    
    for( i = 0; i < stts->entry_count; i++ ) {
        if( (*stts_entry)[i].sample_delta == delta){
            (*stts_entry)[i].sample_count = (*stts_entry)[i].sample_count + sw32( 1 );
            find = 1;
        }
    }
    if( find == 0 ) {
        stts->entry_count = stts->entry_count +  1 ;
        *stts_entry = (STTS_ENTRY *)realloc(stts_entry, sizeof(STTS_ENTRY) * stts->entry_count);
        (*stts_entry)[stts->entry_count].sample_count = sw32( 1 );
        (*stts_entry)[stts->entry_count].sample_delta = sw32(delta);
    }
    stts->entry_count = sw32( stts->entry_count );
}

//获取每段sample的nalu
int get_nalu(FILE *cmsFile, char *buf, int DataSize, int *state) {
    int pos;
    memset(buf, 0, DATA_BUF_SIZE);
    pos = fread(buf, 1, 4, cmsFile);
    if(pos < 0) {
        printf("读取H264码流分隔符失败 fread：%d ",pos);
        return -1;
    }
    pos = 4;
    DataSize = DataSize - pos;
    while(1) {
        if( feof(cmsFile) ) {
            *state = FILE_END;
            break;
        }
        if(DataSize <= 0) {
            *state = FILE_END;
            break;
        }
        buf[pos] = fgetc(cmsFile);
        DataSize = DataSize - 1;
        if( buf[pos - 3] == 0 && buf[pos - 2] == 0 && buf[pos - 1] == 0 && buf[pos] == 1 ) {
            fseek(cmsFile, -4, SEEK_CUR);
            DataSize = DataSize + 4;
            pos = pos - 4;
            break;
        }
        pos++;
    }
    return pos + 1;
    
}
//写入sample数据 返回下次数据的偏移量
int write_sample(FILE *cmsFile, FILE *mp4File, int DataSize, BOX_AVCC *avcC) {
    unsigned char *buf = malloc(DATA_BUF_SIZE);
    unsigned char *nalu = NULL;
    unsigned char naluType;
    unsigned int  offset = ftell(cmsFile);  //保存当前数据的吸入位置
    int len = 0;
    int state = 0;
    while( state == 0 ) {
        len = get_nalu(cmsFile, buf, DataSize, &state);
        if(len < 0) { break; }
        nalu = buf + 4;
        naluType = nalu[0] & 0x1f;
        switch( naluType ) {
        case 0x07:
            if(FristWirteSample) {
                //收集spsx信息
                avcC->AVCProFileIndication  = nalu[1];
                avcC->profile_compatibility = nalu[2];
                avcC->AVCLevelIndication    = nalu[3];
                avcC->lengthSizeMinusOne    = 3;
                avcC->sps_count             = 1;
                uint16_t sps_size               = len - 4;
                avcC->sps_size              = sw16(sps_size);
                avcC->sps                   = strdup(nalu);
                
            }
            buf[0] = ( len >> 24 ) & 0xff;
            buf[1] = ( len >> 16 ) & 0xff;
            buf[2] = ( len >> 8  ) & 0xff;
            buf[3] = ( len >> 0  ) & 0xff;
            fwrite(buf, len, 1, mp4File);
            break;
        case 0x06:
            if(FristWirteSample) {
                //收集pps信息
                avcC->pps_count             = 1;
                uint16_t pps_size           = len - 4;
                avcC->pps_size              = sw16(pps_size);
                avcC->pps                   = strdup(nalu);
                FristWirteSample = false; 
            }
            buf[0] = ( len >> 24 ) & 0xff;
            buf[1] = ( len >> 16 ) & 0xff;
            buf[2] = ( len >> 8  ) & 0xff;
            buf[3] = ( len >> 0  ) & 0xff;
            fwrite(buf, len, 1, mp4File);
            break;

        default:
            buf[0] = ( len >> 24 ) & 0xff;
            buf[1] = ( len >> 16 ) & 0xff;
            buf[2] = ( len >> 8  ) & 0xff;
            buf[3] = ( len >> 0  ) & 0xff;
            fwrite(buf, len, 1, mp4File);
            
        }
    }
    free(buf);

    return offset;
}



int create_chunk(BOX_STSC *stsc, STSC_ENTRY **stsc_entry) {
    if(stsc->entry_count == 0) {
        (*stsc_entry)->first_chunk               = sw32(1);
        (*stsc_entry)->sample_per_chunk          = sw32(1);
        (*stsc_entry)->sample_description_index  = sw32(1);
        stsc->entry_count                        = sw32(1);
    }else{
        uint32_t count = sw32(stsc->entry_count);
        count++;
        *stsc_entry = (STSC_ENTRY *)realloc(*stsc_entry, sizeof(STSC_ENTRY) * count);
        (*stsc_entry)[count -1].first_chunk               = sw32(count);
        (*stsc_entry)[count -1].sample_per_chunk          = sw32(1);
        (*stsc_entry)[count -1].sample_description_index  = sw32(1);
    }

}
int add_chunk_offset(BOX_STCO *stco,  uint32_t **chunk_offset, uint32_t offset) {
    if(stco->entry_count == 0){
        stco->entry_count   = sw32(1);
        (*chunk_offset)[0]  = sw32(offset);
    }else{
        uint32_t count = sw32(stco->entry_count);
        count++;
        //*chunk_offset  = (uint32_t *)realloc(*)
        
    }

}
/* mp4文件中的box是网络字节序 写入数据的时候需要先转换字节序 */
int cms2mp4(FILE *cmsFile, char *mp4Name){
    //一级box
    BOX_FTYP ftyp;
    memset(&ftyp, 0, sizeof(BOX_STTS));

    BOX      mdat;
    memset(&mdat, 0, sizeof(BOX));
    strncpy( (char *)&(mdat.boxType), BOX_TYPE_MDAT, 4 );

    BOX      moov;
    memset(&moov, 0, sizeof(BOX));
    strncpy( (char *)&(moov.boxType), BOX_TYPE_MOOV, 4 );

    //二级 在moov下
    BOX_MVHD mvhd;
    memset(&mvhd, 0, sizeof(BOX_MVHD));
    strncpy( (char *)&(mvhd.header.boxType), BOX_TYPE_MVHD, 4 );

    BOX      trak;
    memset(&trak, 0, sizeof(BOX));
    strncpy( (char *)&(trak.boxType), BOX_TYPE_TRAK, 4 );

    //三级 在trak下
    BOX_TKHD tkhd;
    memset(&tkhd, 0, sizeof(BOX_TKHD));
    strncpy( (char *)&(tkhd.header.boxType), BOX_TYPE_TKHD, 4 );

    BOX      mdia;
    memset(&mdia, 0, sizeof(BOX));
    strncpy( (char *)&(mdia.boxType), BOX_TYPE_MDIA, 4 );

    //四级 在mdia下
    BOX_MDHD mdhd;
    memset(&mdhd, 0, sizeof(BOX_MDHD));
    strncpy( (char *)&(mdhd.header.boxType), BOX_TYPE_MDHD, 4 );

    BOX_HDLR hdlr;
    memset(&hdlr, 0, sizeof(BOX_HDLR));
    strncpy( (char *)&(hdlr.header.boxType), BOX_TYPE_MDAT, 4 );

    BOX      minf;
    memset(&minf, 0, sizeof(BOX));
    strncpy( (char *)&(minf.boxType), BOX_TYPE_MINF, 4 );

    //五级 在minf下
    BOX_VMHD vmhd;
    memset(&vmhd, 0, sizeof(BOX_VMHD));
    strncpy( (char *)&(vmhd.header.boxType), BOX_TYPE_VMHD, 4 );

    BOX      dinf;
    memset(&dinf, 0, sizeof(BOX));
    strncpy( (char *)&(dinf.boxType), BOX_TYPE_DINF, 4 );

    BOX      stbl;
    memset(&stbl, 0, sizeof(BOX));
    strncpy( (char *)&(stbl.boxType), BOX_TYPE_STBL, 4 );

    //六级 在stbl下
    BOX_STSD stsd;
    memset(&stsd, 0, sizeof(BOX_STSD));
    strncpy( (char *)&(stsd.header.boxType), BOX_TYPE_STSD, 4 );

    BOX_STTS stts;
    memset(&stts, 0, sizeof(BOX_STTS));
    strncpy( (char *)&(stts.header.boxType), BOX_TYPE_STTS, 4 );

    BOX_STSS stss;
    memset(&stss, 0, sizeof(BOX_STSS));
    strncpy( (char *)&(stss.header.boxType), BOX_TYPE_STSS, 4 );

    BOX_STSC stsc;
    memset(&stsc, 0, sizeof(BOX_STSC));
    strncpy( (char *)&(stsc.header.boxType), BOX_TYPE_STSC, 4 );

    BOX_STSZ stsz;
    memset(&stsz, 0, sizeof(BOX_STSZ));
    strncpy( (char *)&(stsz.header.boxType), BOX_TYPE_STSZ, 4 );

    BOX_STCO stco;
    memset(&stco, 0, sizeof(BOX_STCO));
    strncpy( (char *)&(stco.header.boxType), BOX_TYPE_STCO, 4 );

    //七级 在stsd下
    BOX_AVC1 avc1;
    memset(&avc1, 0, sizeof(BOX_AVC1));
    strncpy( (char *)&(avc1.header.boxType), BOX_TYPE_AVC1, 4 );

    //八级 在avc1下
    BOX_AVCC avcC;
    memset(&avcC, 0, sizeof(BOX_AVCC));
    strncpy( (char *)&(avcC.header.boxType), BOX_TYPE_AVCC, 4 );
    

    
    
    FILE *mp4File = fopen( mp4Name, "wb" );
    if(mp4File == NULL) {
        printf("创建MP4文件失败");
    }
    //写入mp4头部
    ftyp.header.boxSize = sw32( sizeof( ftyp ) );
    strncpy( (char *)&(ftyp.header.boxType), BOX_TYPE_FTYP, 4 );
    FTYP_BRANDS f_isom = { "isom" };
    FTYP_BRANDS f_iso2 = { "iso2" };
    FTYP_BRANDS f_avc1 = { "avc1" };
    FTYP_BRANDS f_mp41 = { "mp41" };
    ftyp.minor_version         = sw32( 512 );
    ftyp.major_brand           = f_isom;
    ftyp.compatible_brands[0]  = f_isom;
    ftyp.compatible_brands[1]  = f_iso2;
    ftyp.compatible_brands[2]  = f_avc1;
    ftyp.compatible_brands[3]  = f_mp41;
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
    int number = 1;    //记录帧的序号
    int sample_i = 0;  //记录当前已添加关键帧序号
    uint32_t   *sample_number = (uint32_t *)calloc(1, sizeof(uint32_t));

    //stco chunk_offset  记录chunk的偏移量
    uint32_t   *chunk_offset  = (uint32_t *)calloc(1, sizeof(uint32_t));
    
    
    
//--------------------------------

    char ch;   
    char line_buf[1024];
    int last = 0,now = 0;
    uint32_t offset         = 0;
    int DataSize            = 0;
    uint8_t flag            = 0;
    char *boundary          = NULL;
    CMS_STATE state         = CMS_FILE_HEADER;
    int line_buf_ptr        = 0;
    int boundary_ptr        = 0;
    int boundary_size       = 0;
    CMS_DATA_TYPE DataType  = UNKNOW;
    
    while ( !feof( cmsFile ) ) {
        ch = fgetc( cmsFile );       
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
                            get_information(value, &tkhd);
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
                    printf("line_buf_ptr过长\n");
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
                    fseek(cmsFile, -2, SEEK_CUR);
                    break;
                }
                printf("boundary数据错误\n");
                return -1;
            }
            break;
        case CMS_DATA_HEADER:
            if ( ch == '\r' ) {
            } else if ( ch == '\n' ) {
                if ( line_buf_ptr == 0 ) {       
                    switch( DataType ) {
                    case CMS_VEDIO_I:
                    case CMS_VEDIO_P:
                        offset = write_sample(cmsFile, mp4File, DataSize, &avcC);
                        if(offset < 0){
                            return -1;
                        } else {
                            if(DataType == CMS_VEDIO_I){
                                create_chunk(&stsc, &stsc_entry);
                                add_chunk_offset(&stco, &chunk_offset, offset);
                            }else{
                                
                            }
                        }
                        state = CMS_BOUNDARY;
                        break;
                    case CMS_AUDIO:{
                        fseek(cmsFile, 480, SEEK_CUR);
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
                            DataType = CMS_VEDIO_I;
                            if(sample_i == 0) {
                                sample_number[0] = sw32(number);
                                sample_i++;
                            } else {
                                sample_i++;
                                sample_number = (uint32_t *)realloc(sample_number, sizeof(uint32_t) * sample_i);
                                sample_number[sample_i - 1] = sw32(number);
                            }
                            number++;
                        } else if (strcmp(value, "p") == 0) {
                            DataType = CMS_VEDIO_P;
                            number++;
                        } else if (strcmp(value, "a") == 0) {
                            DataType = CMS_AUDIO;
                        } else if (strcmp(value, "j") == 0) {
                        }
                    } else if (strcmp(key, "ts") == 0) {
                        if(DataType == CMS_VEDIO_I || DataType == CMS_VEDIO_I) {
                            last = now;
                            now  = strtol(value, NULL, 10);
                            uint32_t delta = now - last;
                            printf("delta : %d \n", delta);
                            add_delta(delta, &stts_entry, &stts);
                        }
                    } else if (strcmp(key, "l") == 0) {
                        if(DataType == CMS_VEDIO_I || DataType == CMS_VEDIO_I) {
                            DataSize = strtol(value, NULL, 10);
                            stsz.sample_count = add_sample_size(stsz.sample_count, &sample_sizes, DataSize);
                            int i = sw32(stsz.sample_count);
                            
                        }
                        
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
    
    
//------------
    return 0;
}

