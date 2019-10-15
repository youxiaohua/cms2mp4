#include "2MP4.h"



//读取到track信息时 获取视频的信息
int get_information(char *value, BOX_TKHD *tkhd){
    // printf("get_information\n");
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
    //printf("add_sample_size\n");
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
    //printf("add_delta\n");
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
uint32_t write_sample(FILE *cmsFile, FILE *mp4File, int DataSize, BOX_AVCC *avcC) {
    unsigned char *buf = malloc(DATA_BUF_SIZE);
    unsigned char *nalu = NULL;
    unsigned char naluType;
    unsigned int  offset = ftell(mp4File);  //保存当前数据的吸入位置
    //printf("offset: %d\n", offset);
    int len = 0;
    int state = 0;
    while( state == 0 ) {
        len = get_nalu(cmsFile, buf, DataSize, &state);
        if(len < 0) { break; }
        DataSize = DataSize - len;
        nalu = buf + 4;
        naluType = nalu[0] & 0x1f;
        len = len - 4;
        switch( naluType ) {
        case 0x07:
            if(FristWirteSample) {
                //收集spsx信息
                avcC->AVCProFileIndication  = nalu[1];
                avcC->profile_compatibility = nalu[2];
                avcC->AVCLevelIndication    = nalu[3];
                avcC->lengthSizeMinusOne    = 3;
                avcC->sps_count             = 1;
                uint16_t sps_size           = len;
                avcC->sps_size              = sw16(sps_size);
                avcC->sps                   = strdup(nalu);
                
            }
            buf[0] = ( len >> 24 ) & 0xff;
            buf[1] = ( len >> 16 ) & 0xff;
            buf[2] = ( len >> 8  ) & 0xff;
            buf[3] = ( len >> 0  ) & 0xff;
            fwrite(buf, len + 4, 1, mp4File);
            break;
        case 0x06:
            if(FristWirteSample) {
                //收集pps信息
                avcC->pps_count             = 1;
                uint16_t pps_size           = len;
                avcC->pps_size              = sw16(pps_size);
                avcC->pps                   = strdup(nalu);
                FristWirteSample = false; 
            }
            buf[0] = ( len >> 24 ) & 0xff;
            buf[1] = ( len >> 16 ) & 0xff;
            buf[2] = ( len >> 8  ) & 0xff;
            buf[3] = ( len >> 0  ) & 0xff;
            fwrite(buf, len + 4, 1, mp4File);
            break;

        default:
            buf[0] = ( len >> 24 ) & 0xff;
            buf[1] = ( len >> 16 ) & 0xff;
            buf[2] = ( len >> 8  ) & 0xff;
            buf[3] = ( len >> 0  ) & 0xff;
            fwrite(buf, len + 4, 1, mp4File);
            
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
        stsc->entry_count                                 = sw32(count);
    }

}
int add_chunk_offset(BOX_STCO *stco,  uint32_t **chunk_offset, uint32_t offset) {
    if(stco->entry_count == 0){
        stco->entry_count   = sw32(1);
        (*chunk_offset)[0]  = sw32(offset);
    }else{
        uint32_t count = sw32(stco->entry_count);
        
        count++;
        *chunk_offset  = (uint32_t *)realloc(*chunk_offset, sizeof(uint32_t) * count);
        (*chunk_offset)[count - 1] = sw32(offset);
        stco->entry_count = sw32(count);
    }

}

int write_stsd(FILE *mp4File, BOX_AVCC *avcC, uint16_t width, uint16_t height) {
    BOX_AVC1 avc1;
    int avc1_offset = ftell(mp4File);
    memset(&avc1, 0, sizeof(BOX_AVC1));
    strncpy( (char *)&(avc1.header.boxType), "avc1", 4 );
    avc1.width                = sw16(width);
    avc1.height               = sw16(height);
    avc1.data_reference_index = sw32(1);
    avc1.hrsl                 = sw32(0x00480000);
    avc1.vtsl                 = sw32(0x00480000);
    avc1.frame_count          = sw16(1);
    avc1.depth                = sw16(0x0018);
    avc1.pre_defined          = 0xffff; //-1
   
    int avcC_size = sizeof(BOX_AVCC) + avcC->sps_size + avcC->pps_size - 8;
    avc1.header.boxSize       = sw32( sizeof(avc1) );
    fwrite( (char *)&avc1, sizeof(avc1), 1, mp4File);
    fwrite( (char *)avcC, 17, 1, mp4File);
    fwrite( (char *)avcC->sps, sw16(avcC->sps_size), 1, mp4File);
    fwrite( (char *)avcC->pps, sw16(avcC->pps_size), 1, mp4File);
    
    return avc1_offset;
    
}

    
/* mp4文件中的box是网络字节序 写入数据的时候需要先转换字节序 */
int cms2mp4(FILE *cmsFile, char *mp4Name){
    //一级box
    BOX_FTYP ftyp;
    memset(&ftyp, 0, sizeof(BOX_STTS));

    BOX_LARGE mdat;
    memset(&mdat, 0, sizeof(BOX_LARGE));
    mdat.boxSize = sw32(1);
    strncpy( (char *)&(mdat.boxType), "mdat", 4 );
   
    BOX_TKHD tkhd;
    memset(&tkhd, 0, sizeof(BOX_TKHD));
    strncpy( (char *)&(tkhd.header.boxType), "tkhd", 4 );
   
    BOX_AVCC avcC;
    memset(&avcC, 0, sizeof(BOX_AVCC));
    strncpy( (char *)&(avcC.header.boxType), "avcC", 4 );
  
 
    
  

    //八级 在avc1下

    

    
    
    FILE *mp4File = fopen( mp4Name, "wb" );
    if(mp4File == NULL) {
        printf("创建MP4文件失败");
    }
    //写入mp4头部
    ftyp.header.boxSize = sw32( sizeof( ftyp ) );
    strncpy( (char *)&(ftyp.header.boxType), "ftyp", 4 );
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
    fwrite(&mdat, sizeof(mdat), 1, mp4File);
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
    uint32_t   *stss_entry = (uint32_t *)calloc(1, sizeof(uint32_t));

    //stco chunk_offset  记录chunk的偏移量
    uint32_t   *chunk_offset  = (uint32_t *)calloc(1, sizeof(uint32_t));
    
    printf("ftyp : %d\n", ftell(mp4File));
    
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
        case CMS_DATA_HEADER: {
            if ( ch == '\r' ) {
            } else if ( ch == '\n' ) {
                if ( line_buf_ptr == 0 ) {       
                    switch( DataType ) {
                    case CMS_VEDIO_I:
                    case CMS_VEDIO_P:
                        /*while(DataSize > 0){
                            ch = fgetc(cmsFile);
                            DataSize--;
                            }*/
                        offset = write_sample(cmsFile, mp4File, DataSize, &avcC);
                        if(offset < 0){
                            return -1;
                        } else {
                            if(DataType == CMS_VEDIO_I){
                                create_chunk(&stsc, &stsc_entry);
                                add_chunk_offset(&stco, &chunk_offset, offset);
                            }else{
                                int count = sw32(stsc.entry_count) - 1;
                                stsc_entry[ count ].sample_per_chunk = stsc_entry[ count ].sample_per_chunk + sw32(1);
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
                                stss_entry[0] = sw32(number);
                                sample_i++;
                            } else {
                                sample_i++;
                                stss_entry = (uint32_t *)realloc(stss_entry, sizeof(uint32_t) * sample_i);
                                stss_entry[sample_i - 1] = sw32(number);
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
                        if(DataType == CMS_VEDIO_I || DataType == CMS_VEDIO_P) {
                            last = now;
                            now  = strtol(value, NULL, 10);
                            uint32_t delta = now - last;
                            //printf("%s  last : %d   now : %d delta : %d \n", value, last, now, delta);
                            add_delta(delta, &stts_entry, &stts);
                        }
                    } else if (strcmp(key, "l") == 0) {
                        if(DataType == CMS_VEDIO_I || DataType == CMS_VEDIO_P) {
                            DataSize = strtol(value, NULL, 10) ;
                            //printf("value : %s DataSize : %d\n", value, DataSize);
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
    }

//视频数据读取完 整理数据填入box中并写入mp4File中

    //fseek回到mdat的largeBoxSize 写入数据的大小
    long int mdat_size = ftell(mp4File) - sizeof(ftyp);
    mdat_size = sw64(mdat_size);
    fseek(mp4File, sizeof(ftyp) + sizeof(BOX) , SEEK_SET);
    fwrite((char *)&mdat_size, sizeof(long int), 1, mp4File);
    fseek(mp4File, 0, SEEK_END);


    //写入moov
    BOX      moov;
    memset(&moov, 0, sizeof(BOX));
    strncpy( (char *)&(moov.boxType), "moov", 4 );

    int moov_offset = ftell(mp4File);
    fwrite(&moov, sizeof(moov), 1, mp4File);

    //写入mvhd
    BOX_MVHD mvhd;
    memset(&mvhd, 0, sizeof(BOX_MVHD));
    strncpy( (char *)&(mvhd.header.boxType), "mvhd", 4 );
    
    mvhd.time_scale     = sw32(1000);      //时间刻度
    mvhd.duration       = sw32(now);       //时间长度  now保存着播放最后一帧的时间
    mvhd.rate           = sw32(0x00010000);//播放速度
    mvhd.volume         = sw16(0x0100);    //播放音量
    mvhd.matrix[0]      = mvhd.rate;
    mvhd.matrix[4]      = mvhd.rate;
    mvhd.matrix[8]      = sw32(0x40000000);
    mvhd.next_trak_id  = sw32(2);         //下一个tack使用的id
    mvhd.header.boxSize = sw32(sizeof(mvhd));
    fwrite(&mvhd, sizeof(mvhd), 1, mp4File);

    /*-----写入trak-----*/
    BOX trak;
    memset(&trak, 0, sizeof(BOX));
    strncpy( (char *)&(trak.boxType), "trak", 4 );
    
    int trak_offset = ftell(mp4File);
    fwrite(&trak, sizeof(trak), 1, mp4File);


    //tkhd
    
    tkhd.header.boxSize = sw32(sizeof(tkhd));
    tkhd.full_box.flags = 0x0f0000;  //转换成大端序后的值 原值为0x00000f 位或操作 设置改trak是否播放等信息 
    tkhd.track_id       = sw32(1);
    tkhd.duration       = sw32(now);
    tkhd.volume         = sw16(0x0100);
    tkhd.matrix[0]      = mvhd.rate;
    tkhd.matrix[4]      = mvhd.rate;
    tkhd.matrix[8]      = sw32(0x40000000);
    fwrite(&tkhd, sizeof(tkhd), 1, mp4File);
    
    //edts
    BOX edts;
    EDTS_ELST elst;
    EDIT_ENTRY edit;
    memset(&edts, 0, sizeof(BOX));
    memset(&elst, 0, sizeof(elst));
    memset(&edit, 0, sizeof(edit));
    strncpy( (char *)&(edts.boxType), "edts", 4 );
    strncpy( (char *)&(elst.header.boxType), "elst", 4 );
    elst.edit_count     = sw32(1);
    edit.media_time     = 0xffffffff; // -1
    edit.media_rate     = sw32(1);
    int elst_size       = sizeof(elst) + sizeof(edit);
    elst.header.boxSize = sw32(elst_size);
    int edts_size       = sizeof(edts) + elst_size;
    edts.boxSize        = sw32(edts_size);

    fwrite( (char *)&edts, sizeof(edts), 1, mp4File );
    fwrite( (char *)&elst, sizeof(elst), 1, mp4File );
    fwrite( (char *)&edit, sizeof(edit), 1, mp4File );
    
    
    //mdia
    BOX mdia;
    int mdia_offset = ftell(mp4File);
    memset(&mdia, 0, sizeof(BOX));
    strncpy( (char *)&(mdia.boxType), "mdia", 4 );
    fwrite(&mdia, sizeof(mdia), 1, mp4File);
    
    BOX_MDHD mdhd;
    memset(&mdhd, 0, sizeof(BOX_MDHD));
    strncpy( (char *)&(mdhd.header.boxType), "mdhd", 4 );
    mdhd.time_scale      = sw32(1000);
    mdhd.duration        = sw32(now);
    mdhd.language        = sw16(21956);
    mdhd.header.boxSize  = sw32(sizeof(mdhd));
    /*21956  
      0 10101 01110 00100 
        21    14    4  
      最高位为0 后面每5位为一个字母  数值表示字母的序号 
      21表示u 14表示n 4表示d  参照ISO_639-2/T und表示未定义的语言类型
    */
    fwrite( (char *)&mdhd, sizeof(mdhd), 1, mp4File );


    BOX_HDLR hdlr;
    int hdlr_size;
    memset(&hdlr, 0, sizeof(BOX_HDLR));
    strncpy( (char *)&(hdlr.header.boxType), "hdlr", 4 );
    strncpy( (char *)&(hdlr.handler_type), "vide", 4 );
    hdlr.name           = strdup("VideoHandler");
    hdlr_size           = sizeof(hdlr) - sizeof(int8_t *) + strlen(hdlr.name);
    hdlr.header.boxSize = sw32(hdlr_size);
    fwrite( (char *)&hdlr, sizeof(hdlr) - sizeof(uint8_t *), 1, mp4File );
    fwrite( hdlr.name, strlen(hdlr.name), 1, mp4File);
    free(hdlr.name);

    //minf
    BOX minf;
    int minf_size = ftell(mp4File);
    memset(&minf, 0, sizeof(BOX));
    strncpy( (char *)&(minf.boxType), "minf", 4 );
    fwrite( (char *)&minf, sizeof(minf), 1, mp4File );
    
    
    BOX_VMHD vmhd;
    memset(&vmhd, 0, sizeof(BOX_VMHD));
    strncpy( (char *)&(vmhd.header.boxType), "vmhd", 4 );
    vmhd.full_box.flags = 0x010000;    //这里总是=1
    vmhd.header.boxSize = sw32(sizeof(vmhd));
    fwrite( (char *)&vmhd, sizeof(vmhd), 1, mp4File );

    BOX dinf;
    DREF_URL url;
    DINF_DREF dref;
    int dinf_size;
    int dref_size;
    memset(&dinf, 0, sizeof(BOX));
    memset(&dref, 0, sizeof(dref));
    memset(&url, 0, sizeof(url));
    strncpy( (char *)&(dinf.boxType), BOX_TYPE_DINF, 4 );
    strncpy( (char *)&(url.header.boxType), "url ", 4 );
    strncpy( (char *)&(dref.header.boxType), "dref", 4 );
    dref.entry_count    = sw32(1);
    url.full_box.flags  = 0x010000; 
    url.header.boxSize  = sizeof(url);
    dref_size           = sizeof(url) + sizeof(dref);
    dinf_size           = dref_size + sizeof(dinf);
    dref.header.boxSize = sw32(dref_size);
    dinf.boxSize        = sw32(dref_size);
    fwrite( (char *)&dinf, dinf_size, 1, mp4File );
    fwrite( (char *)&dref, dref_size, 1, mp4File );
    fwrite( (char *)&url, sizeof(url), 1, mp4File );

    BOX stbl;
    int stbl_offset = ftell(mp4File);
    memset(&stbl, 0, sizeof(BOX));
    strncpy( (char *)&(stbl.boxType), "stbl", 4 );
    fwrite( (char *)&stbl, sizeof(stbl), 1, mp4File );
    write_stsd(mp4File, &avcC, (uint16_t)sw32(tkhd.width), (uint16_t)sw32(tkhd.height));
     
    BOX_STTS stts;
    memset(&stts, 0, sizeof(BOX_STTS));
    strncpy( (char *)&(stts.header.boxType), "stts", 4 );
    int stts_entry_count = sw32(stts.entry_count);
    int stts_size        = sizeof(stts) + stts_entry_count * sizeof(STTS_ENTRY);
    fwrite( (char *)&stts, sizeof(stts), 1, mp4File);
    fwrite( (char *)&stts_entry, stts_entry_count * sizeof(STTS_ENTRY), 1, mp4File);

    
    BOX_STSS stss;
    memset(&stss, 0, sizeof(BOX_STSS));
    strncpy( (char *)&(stss.header.boxType), "stss", 4 );
    stss.entry_count = sample_i;
    int stss_size    = sizeof(stss) + sizeof(uint32_t) * sample_i;
    fwrite( (char *)&stss, sizeof(stss), 1, mp4File);
    fwrite( (char *)&stss_entry, sample_i * sizeof(uint32_t), 1, mp4File);
    

    BOX_STSC stsc;
    memset(&stsc, 0, sizeof(BOX_STSC));
    strncpy( (char *)&(stsc.header.boxType), "stsc", 4 );

    BOX_STSZ stsz;
    memset(&stsz, 0, sizeof(BOX_STSZ));
    strncpy( (char *)&(stsz.header.boxType), "stsz", 4 );

    BOX_STCO stco;
    memset(&stco, 0, sizeof(BOX_STCO));
    strncpy( (char *)&(stco.header.boxType), "stco", 4 );

    //七级 在stsd下
   
    
    /*-----trak-----*/
    return 0;
}

int main(int argc, char *argv[])
{
    FILE *fp = fopen("../misc/ttt.cms", "r");
    cms2mp4(fp, "a.mp4");
    fclose(fp);
    return 0;
}
