/*
    usbd.h USB Device For PY32F403
    Could only operate by byte(8b) or word(16b) 
    while writing in 8b/16b and reading in 8b
*/

#include "py32f403_memory.h"

typedef struct {
    __IO uint32_t CR;
    __O  uint32_t INTR; // Interrupt
    __IO uint32_t INTRE; // Interrupt Enable
    __IO uint32_t FRAME;
    
    __IO uint32_t EP0CSR;
    __IO uint32_t INEPxCSR;
    __IO uint32_t OUTEPxCSR;
    __O  uint32_t OUTCOUNT;
    __IO uint32_t FIFODATA[8];
} __PACKED usbd_t;

typedef struct {
	__IO uint8_t ADDR    			  ;//0X00;
	__IO uint8_t POWER    			;//0X01;
	__IO uint8_t REV0[2]			  ;//0X02~03;

	__IO uint8_t INT_USB  			;//0X04;
	__IO uint8_t INT_OUT1 			;//0X05;
	__IO uint8_t INT_IN1  			;//0X06;
	__IO uint8_t REV1[1]			  ;//0X07;

	__IO uint8_t INT_USBE 			;//0X08;
	__IO uint8_t INT_OUT1E			;//0X09;
	__IO uint8_t INT_IN1E 			;//0X0A;
	__IO uint8_t REV2[1]			  ;//0X0B;

	__IO uint16_t FRAME         ;//0X0C-0X0D;
	__IO uint8_t INDEX    			;//0X0E;
	__IO uint8_t REV3[1]			  ;//0X0F;

	__IO uint8_t EP0_CSR   		  ;//0X10;//EP_CSR
	__IO uint8_t EP0_COUNT			;//0X11; //EP_COUNT0
	__IO uint8_t REV4[2]			  ;//0X12-0X13;

	__IO uint8_t IN_CSR2   			;//0X14;
	__IO uint8_t IN_CSR1   			;//0X15;
	__IO uint8_t MAX_PKT_IN   	;//0X16;
	__IO uint8_t REV5[1]			  ;//0X17

	__IO uint8_t OUT_CSR2  			;//0X18;
	__IO uint8_t OUT_CSR1  			;//0X19;
	__IO uint8_t MAX_PKT_OUT  	;//0X1A;
	__IO uint8_t REV6[1]			  ;//0X1B

	__IO uint16_t OUT_COUNT 		;//0X1C-0X1D;
	__IO uint8_t REV7[2]			  ;//0X1E~1F;

	__IO uint8_t FIFO_EP0 			;//0X20;
	__IO uint8_t REV8[3]			  ;//0X21~23;

	__IO uint8_t FIFO_EP1 			;//0X24; 
	__IO uint8_t REV9[3]			  ;//0X25~27;

	__IO uint8_t FIFO_EP2 			;//0X28;
	__IO uint8_t REV10[3]			  ;//0X29~2B;

	__IO uint8_t FIFO_EP3 			;//0X2C;    
	__IO uint8_t REV11[3]			  ;//0X2D~2F;

	__IO uint8_t FIFO_EP4 			;//0X30;
	__IO uint8_t REV12[3]			  ;//0X31~33;

	__IO uint8_t FIFO_EP5 			;//0X34; 
	__IO uint8_t REV13[3]			  ;//0X25~37;

	__IO uint8_t FIFO_EP6 			;//0X38;
	__IO uint8_t REV14[3]			  ;//0X39~3B;

	__IO uint8_t FIFO_EP7 			;//0X3C;    
} usb_reg_t;

#ifdef __USBD_REG_ENABLED
#define usbd    ((usb_reg_t *) USBD_Base)
#else
#define usbd    ((usbd_t *) USBD_Base)
#endif


