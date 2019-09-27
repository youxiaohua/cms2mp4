#include "2MP4.h"


/* mp4文件中的box是网络字节序 写入数据的时候需要先转换字节序 */
int cms2mp4(FILE *cmsFile, char *mp4Name){
    FILE *mp4File = fopen( mp4Name, "wb" );
    //写入mp4头部
    BOX_FTYP ftyp;
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
    BOX mdat;
    


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
