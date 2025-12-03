#include <stdio.h>
int main(int argc,char ** argv) {if(argc==1) return -1; int c, p=0; printf( "static const unsigned char %s[] = {", argv[1] ); while( ( c = getchar() ) != EOF ) printf( "0x%02x,%c", c, (((p++)&15)==15)?10:' '); printf( "};" ); return 0; }
