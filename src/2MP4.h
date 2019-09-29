#ifndef TMP4_H
#define TMP4_H

#include<stdio.h>
#include<stdint.h>
#include<string.h>

#define sw32(x)  ( ( ( (uint32_t)(x) & 0xff000000 ) >> 24 ) | \
                   ( ( (uint32_t)(x) & 0x00ff0000 ) >> 8  ) | \
                   ( ( (uint32_t)(x) & 0x0000ff00 ) << 8  ) | \
                   ( ( (uint32_t)(x) & 0x000000ff ) << 24 ) )

#define sw16(x)  ( ( ( (uint16_t)(x) & 0xff00 ) >> 8 ) | \
                   ( ( (uint16_t)(x) & 0x00ff ) << 8 ) )

#define BOX_TYPE_FTYP "ftyp"
#define BOX_TYPE_MOOV "moov"
#define BOX_TYPE_MVHD "mvhd"
#define BOX_TYPE_TRAK "trak"
#define BOX_TYPE_TKHD "tkhd"
#define BOX_TYPE_EDTS "edts"
#define BOX_TYPE_MDIA "mdia"
#define BOX_TYPE_MDHD "mdhd"
#define BOX_TYPE_HDLR "hdlr"
#define BOX_TYPE_MINF "minf"
#define BOX_TYPE_VMHD "vmhd"
#define BOX_TYPE_DINF "dinf"
#define BOX_TYPE_DREF "dref"
#define BOX_TYPE_STBL "stbl"
#define BOX_TYPE_STSD "stsd"
#define BOX_TYPE_STTS "stts"
#define BOX_TYPE_STSS "stss"
#define BOX_TYPE_STSC "stsc"
#define BOX_TYPE_STSZ "stsz"
#define BOX_TYPE_STCO "stco"
#define BOX_TYPE_UDTA "udta"



#define MAX_BOX_TYPE_LEN 4
#define MAX_FTYP_BRANDS_LEN 4
#define MAX_FTYP_BRANDS_NUM 4

#pragma pack(push)
#pragma pack(1)
typedef struct box{
    uint32_t boxSize;                      //如果为1 表示box的大小信息需要在largeBoxSize取得 一般只有mdat才会用到
    uint8_t  boxType[MAX_BOX_TYPE_LEN];     //box的类型 4个字符
}BOX;

typedef struct box_large{
    uint32_t boxSize;                      //如果为1 表示box的大小信息需要在largeBoxSize取得 
    uint8_t  boxType[MAX_BOX_TYPE_LEN];     //box的类型 4个字符
    uint64_t largeBoxSize;
}BOX_LARGE;

typedef struct full_box{
    uint32_t version : 8;              //box版本0或1 版本不同数据字节数不同
    uint32_t flags   : 24;             //标志
}FULL_BOX;

//协议     
typedef struct ftyp_brands{
    unsigned char brands[MAX_FTYP_BRANDS_LEN];
}FTYP_BRANDS;

typedef struct box_ftyp{
    BOX         header;
    FTYP_BRANDS major_brand;           //主要的协议
    uint32_t    minor_version;                              //版本号
    FTYP_BRANDS compatible_brands[MAX_FTYP_BRANDS_NUM];     //遵从的协议（ismo, iso2, mp41, acv1 ...）
}BOX_FTYP;



typedef struct box_mvhd{
    BOX      header;
    FULL_BOX full_box;
    uint32_t creation_time;
    uint32_t modification_time;
    uint32_t time_scale;              //时间刻度  
    uint32_t duration;                //时间长度  视频时长=时间长度/时间刻度
    uint32_t rate;                    //播放速度 高16表示整数  低16位表示小数 默认0x00010000
    uint16_t volume;                  //播放音量 高8位表示整数 低8位表示小数 默认0x0100
    uint8_t  reserved[10];            //保留位
    uint32_t matrix[9];               //视频变换矩阵
    uint32_t pre_defined[6];          
    uint32_t next_track_id;           //下一个track使用的id
}BOX_MVHD;

typedef struct box_tkhd{
    BOX      header;
    FULL_BOX full_box;
    uint32_t creation_time;
    uint32_t modification_time;
    uint32_t track_id;
    uint32_t reserved1;               //保留位
    uint32_t duration;                //时间长度  视频时长=时间长度/时间刻度
    uint64_t reserved;                //保留位
    uint16_t layer;                   //视频层 默认0,  值小在上层
    uint16_t alternate_group;         //track分组信息,默认0 表示与其他track没有群组关系
    uint16_t volume;                  //播放音量 高8位表示整数 低8位表示小数 默认0x0100
    uint16_t reserved2;               //保留位
    uint32_t matrix[9];               //视频变换矩阵
    uint32_t width;                   //
    uint32_t height;                  //
}BOX_TKHD;



typedef struct box_mdhd{
    BOX      header;
    FULL_BOX full_box;
    uint32_t creation_time;
    uint32_t modification_time;
    uint32_t time_scale;
    uint32_t duration;                //时间长度  视频时长=时间长度/时间刻度
    uint16_t language;                //媒体语言码,最高位为0 后面15位为3个字符
    uint16_t pre_defined;             //保留位
}BOX_MDHD;


typedef struct box_hdlr{
    BOX      header;
    FULL_BOX full_box;
    uint32_t pre_defined;
    uint32_t handler_type;            //4个字符"vide" "soun" "hint"
    uint8_t  reserved[12];            //保留位
    uint8_t  name[13];                //track的名字
}BOX_HDLR;




//minf下的Media Information Header Box  根据不同的数据类型使用不同的box

typedef struct box_vmhd{//视频
    BOX      header;
    FULL_BOX full_box;
    uint16_t graphics_mode;           //视频合成模式，为0时拷贝原始图像，否则与opcolor进行合成
    uint16_t opcolor[3];              //red, green, blue
}BOX_VMHD;



typedef struct box_smhd{//音频
    BOX      header;
    FULL_BOX full_box;
    int16_t  balance;                 //8.8格式 一般为0 -1.0表示左声道  1.0表示右声道
    uint16_t reserved;
}BOX_SMHD;



/*dinf
  -dref
  --url
  如果track被分为若干段, 用来定位track数据 每一段url指向地址来获取数据
*/
typedef struct box_dref{
    BOX      header;
    FULL_BOX full_box;           
    uint32_t entry_count;              //"url"或"urn"表的元素个数 
}BOX_FREF;

typedef struct box_url{
    BOX      header;
    FULL_BOX full_box;                 //flag 值为1 表示url字符串为空
    //uint8_t ulr_data[n];             //
}BOX_URL;


//  stsd 存放音视频解码信息 根据当前的track数据类型 分为avc1(视频) mp4a(音频)
typedef struct box_stsd{
    BOX      header;
    FULL_BOX full_box;
    uint32_t entry_count;
}BOX_STSD;

typedef struct box_avc1{//视频解码信息
    BOX      header;
    uint8_t  resved[6];               //保留字段
    uint16_t data_reference_index;    //数据查询索引  与stsc 的 sample_description_index关联 
    uint8_t  resved2[16];             //保留字段
    uint16_t windth;
    uint16_t height;
    uint32_t hrsl;                    //水平分辨率0x00480000 表示72dpi
    uint32_t vtsl;                    //垂直分辨率0x00480000
    uint32_t reserved;                //一直为0
    uint16_t frame_count;             //每个采样里面的帧数 视频必须为1
    int8_t   compressorname[32];      //无用  第一个字节便是字符长度 剩下31个表示内容
    uint16_t depth;                   //色深 0x0018 24位
    int16_t  pre_defined;             //-1
}BOX_AVC1;

typedef struct box_mp4a{//音频解码信息
    BOX      header;
    FULL_BOX full_box;
}BOX_MP4A;



typedef struct box_avcC{//在avc1里面, 视频的解码信息里面有sps pps等信息
    uint8_t  configurationVersion;      //版本号
    uint8_t  AVCProFileIndication;      //sps[1]
    uint8_t  profile_compatibility;     //sps[2]
    uint8_t  AVCLevelIndication;        //sps[3]
    uint8_t  reserved1 : 6;             //保留字段
    uint8_t  lengthSizeMinusOne : 2;    //非常重要 NALU 的长度-1  计算方法是 1 + (lengthSizeMinusOne & 3)，实际计算结果一直是4
    uint8_t  reserved2 : 3;
    uint8_t  sps_count : 5;             //sps个数
    uint16_t sps_size;
    uint8_t  *sps;                      //sps 
    uint8_t  pps_count;                 //pps个数
    uint16_t pps_size;
    uint8_t  *pps;                      //pps
}BOX_AVCC;


//stts

typedef struct box_stts_entry{
    uint32_t sample_count;
    uint32_t sample_delta;
}STTS_ENTRY;
    
typedef struct box_stts{//时间间隔和sample数量
    BOX         header;
    int         entry_count;          
    STTS_ENTRY  *entry;
    
}BOX_STTS;


typedef struct box_stsz{//包含sample的数量和每个sample的字节大小
    BOX         header;
    FULL_BOX    full_box;
    uint32_t    sample_size;          //全部sample的数据，如果所有的sample有相同的长度,这个字段就是这个值,否则为0          
    uint32_t    sample_count;        
    uint32_t    *sample_sizes;
}BOX_STSZ;

typedef struct stsc_entry{
    uint32_t first_chunk;              //当前第一个chunk的
    uint32_t samples_per_chunk;        //每个chunk拥有的sample 
    uint32_t sample_description_index; //采样描述索引  在stsd中找到描述信息
}STSC_ENTRY;

typedef struct box_stsc{//media中的sample被分组成chunk,包含sample-chunk的映射表
    BOX        header;
    FULL_BOX   full_box;
    uint32_t   entry_count;
    STSC_ENTRY *entry;
}BOX_STSC;

typedef struct box_stss{//包含media中的关键帧的sample表,此表不存在便是每一个sample都是关键帧
    BOX        header;
    FULL_BOX   full_box;
    uint32_t   *sample_number;            //sample关键帧的序号
}BOX_STSS;

typedef struct box_stco{//储存每个chunk在文件的位置
    BOX        header;
    FULL_BOX   full_box;
    uint32_t   entry_count;
    uint32_t   *chunk_offset;          //chunk在文件中的偏移量
}BOX_STCO;

typedef struct box_stco64{//当32位不够存放chunk偏移量时使用
    BOX        header;
    FULL_BOX   full_box;
    uint32_t   entry_count;
    uint64_t   *chunk_offset;          //chunk在文件中的偏移量
}BOX_STCO64;

#pragma pack(pop)

typedef enum{
    CMS_FILE_HEADER = 0,
    CMS_BOUNDARY,
    CMS_DATA_HEADER,
}CMS_STATE;
typedef enum{
    UNKNOW = -1,
    CMS_VEDIO = 0,
    CMS_AUDIO,
}CMS_DATA_TYPE;

int cms2mp4(FILE *cmsFile, char *mp4Name);
int write_ftyp(FILE *mp4File);
int write_mdat(FILE *cmsFile, FILE *mp4File);






#endif
