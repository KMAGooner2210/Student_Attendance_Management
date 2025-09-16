/**--------------He thong quan ly diem danh sinh vien--------------**/
/**--------------KMAGooner2210 -- Mai Tung--------------**/
/**--------------So do noi chan--------------**/
/**		STM32F103	 --------------		RFID RC522**/
/**		3.3V	 		 --------------		3.3V**/
/**		GND     	 --------------		GND**/
/**		A4	     	 --------------		SDA**/
/**		A5	     	 --------------		SCK**/
/**		A7	     	 --------------		MOSI**/
/**		A6	     	 --------------		MISO**/
/**		A3	     	 --------------		RST**/


#include "stm32f10x.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_rcc.h"
#include "misc.h"
#include "rc522.h"
#include <stdio.h>
#include <string.h>

#define FLASH_PAGE_SIZE						((uint16_t)0x400)				// 1KB
#define ATTENDANCE_START_ADDR 		((uint32_t)0x0800FC00)	// Page 63
#define ATTENDANCE_END_ADDR				((uint32_t)0x0800FFFF)


// Cau truc du lieu diem danh
typedef struct {
	uint32_t timestamp;				// Thoi gian diem danh
	uint8_t studentIndex;			// Index cua sinh vien
	uint8_t status;						// Trang thai (1: co mat, 0: vang)
	uint8_t checksum;					// Checksum de kiem tra loi
} AttendanceRecord;

// Cau truc du lieu sinh vien
typedef struct {
	uint8_t uid[5];			// UID cua the
	char mssv[100];			// Ma so sinh vien
	char name[30];			// Ten sinh vien
} StudentData;


StudentData registeredStudents[] = {
	
	  {{0x77, 0xA1, 0x30, 0xC2, 0x24}, "DT060150", "Mai Thanh Tung"},
    {{0x17, 0x6B, 0x08, 0xC2, 0xB6}, "DT060148", "Ngo Viet Tri"},
    {{0x64, 0xD0, 0xF8, 0x04, 0x48}, "DT060113", "Duong Van Khang"},
    {{0x47, 0x2D, 0x23, 0xC2, 0x8B}, "DT060215", "Nguyen Trung Kien"},
};

uint8_t studentCount = sizeof(registeredStudents) / sizeof(registeredStudents[0]);
uint8_t attendanceList[50]  = {0};
uint32_t recordCount = 0;		// So ban ghi diem danh
char buffer[100];


void USART1_Config(void) {
    USART_InitTypeDef USART_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
    
    //  TX (PA9), RX (PA10)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    
    USART_InitStructure.USART_BaudRate = 9600;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);
    
    USART_Cmd(USART1, ENABLE);
}


void SPI1_Config(void) {
    SPI_InitTypeDef SPI_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1 | RCC_APB2Periph_GPIOA, ENABLE);
    
    //SCK(PA5), MISO(PA6), MOSI(PA7)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    //NSS (PA4) , RST (PA3)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);
    
    SPI_Cmd(SPI1, ENABLE);
}




void USART1_SendChar(char c) {
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, c);
}


void USART1_SendString(char *str) {
    while (*str) {
        USART1_SendChar(*str++);
    }
}

void FLASH_Config(void){
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
}

void FLASH_ERASEPAGE(void){
	FLASH_ErasePage(ATTENDANCE_START_ADDR);
	recordCount = 0;
	
	for(uint8_t i = 0; i < studentCount; i++){
		attendanceList[i] = 0;
	}
	USART1_SendString("Da xoa lich su diem danh!\r\n");
}


uint8_t CalculateChecksum(uint8_t *data, uint8_t length) {
	uint8_t checksum = 0;
	for(uint8_t i = 0; i < length; i++){
		checksum ^= data[i];
}
	return checksum;
}


//Ham ghi ban ghi diem danh vao FLASH
void FLASH_WriteRecord(AttendanceRecord *record){
  uint32_t address = ATTENDANCE_START_ADDR + (recordCount * sizeof(AttendanceRecord));
  
  if (address + sizeof(AttendanceRecord) >= ATTENDANCE_END_ADDR){
    USART1_SendString("Bo nho day! Xoa du lieu cu...\r\n");
		FLASH_ERASEPAGE();
		address = ATTENDANCE_START_ADDR;
}
	
	record->checksum = CalculateChecksum((uint8_t*)record, sizeof(AttendanceRecord) - 1);
  
	uint32_t *pData = (uint32_t*)record;
	for(uint8_t i = 0; i < sizeof(AttendanceRecord) / 4; i++) {
	FLASH_ProgramWord(address + (i * 4), pData[i]);
	}
	
	recordCount++;
}

void FLASH_ReadRecords(void){
	uint32_t address = ATTENDANCE_START_ADDR;
	AttendanceRecord record;
	uint8_t validRecords = 0;
	
	for(uint8_t i = 0; i < studentCount; i++){
		attendanceList[i] = 0;
	}
	
	while(address < ATTENDANCE_END_ADDR){
		uint32_t *pRecord = (uint32_t*)&record;
		
		for(uint8_t i = 0; i < sizeof(AttendanceRecord) / 4; i++){
			pRecord[i] = *(__IO uint32_t*)(address + (i * 4));
		}
		
		if (pRecord[0] == 0xFFFFFFFF) {
			break;
		}
		
             uint8_t calculatedChecksum = CalculateChecksum((uint8_t*)&record, sizeof(AttendanceRecord) - 1);
        
        if (record.checksum == calculatedChecksum && record.studentIndex < studentCount) {
            // C?p nh?t danh sách di?m danh
            attendanceList[record.studentIndex] = record.status;
            validRecords++;
            address += sizeof(AttendanceRecord);
        } else {
            // D?ng khi g?p b?n ghi không h?p l?
            break;
        }
    }
    
    recordCount = validRecords;
    sprintf(buffer, "Da doc ban ghi diem danh tu Flash\r\n", recordCount);
    USART1_SendString(buffer);
}

// Ham so sanh UID
uint8_t UID_Match(uint8_t *uid1, uint8_t *uid2, uint8_t length){
	for(uint8_t i = 0; i < length; i++){
		if(uid1[i] != uid2[i]){
			return 0; 		// Khong khop
		}
	}
	return 1;		// Khop
}

// Ham kiem tra the da dang ky chua
int8_t CheckCardRegistered(uint8_t  *uid, uint8_t uidLength){
	for(uint8_t i = 0; i < studentCount; i++){
		if(UID_Match(uid, registeredStudents[i].uid, uidLength)){
			return i; 		// Tra ve index cua sinh vien
		}
	}
	return -1;			// The chua dang ky
}

// Ham hien thi thong tin sinh vien
void DisplayStudentInfo(uint8_t index){
	sprintf(buffer, "MSSV: %s | Ten: %s\r\n",
					registeredStudents[index].mssv,
					registeredStudents[index].name);
	USART1_SendString(buffer);
}

// Ham diem danh
void MarkAttendance(int8_t studentIndex){
	if(studentIndex >= 0){
		AttendanceRecord record;
		
		record.timestamp = 0;
		record.studentIndex = studentIndex;
		
		if(attendanceList[studentIndex] == 0){
			attendanceList[studentIndex] = 1;
			record.status = 1;
			
			USART1_SendString("----------DIEM DANH THANH CONG----------\r\n");
			DisplayStudentInfo(studentIndex);
			USART1_SendString("----------\r\n");
		}else{
			attendanceList[studentIndex] = 0;
			record.status = 0;
			
			USART1_SendString("----------HUY DIEM DANH----------\r\n");
			DisplayStudentInfo(studentIndex);
			USART1_SendString("----------\r\n");
		}
		
		FLASH_WriteRecord(&record);
		
}else{
		USART1_SendString("THE CHUA DANG KY!\r\n");
	  USART1_SendString("LIEN HE LAI GIAO VIEN DE DANG KY THE.\r\n");
	  USART1_SendString("----------\r\n");
}
}

// Ham hien thi tong hop diem danh
void ShowAttendanceSummary(void){
		uint8_t presentCount = 0;
	
	  USART1_SendString("\r\n=== TONG HOP DIEM DANH ===\r\n");
	  for(uint8_t i = 0; i < studentCount; i++){
			sprintf(buffer, "%s - %s: %s\r\n",
							registeredStudents[i].mssv,
							registeredStudents[i].name,
			attendanceList[i] ? "CO MAT" : "VANG");
			
			if(attendanceList[i]) presentCount++;
		}
		
		sprintf(buffer, "Tong: %d/%d sinh vien co mat\r\n", presentCount, studentCount);
		USART1_SendString(buffer);
		sprintf(buffer, "Tong so ban ghi: %d\r\n", recordCount);
		USART1_SendString(buffer);
    USART1_SendString("=============================\r\n");
}


// Ham xu ly lenh tu UART
void ProcessCommand(char *cmd){
	
	if(strcmp(cmd, "CLEAR") == 0){
		FLASH_ERASEPAGE();
	}else if(strcmp(cmd, "STATUS") == 0){
		ShowAttendanceSummary();
	}else if (strcmp(cmd, "HELP") == 0){
		USART1_SendString("Cac lenh ho tro:\r\n");
		USART1_SendString("CLEAR - Xoa lich su diem danh\r\n");
		USART1_SendString("STATUS - Hien thi trang thai diem danh\r\n");
		USART1_SendString("HELP - Hien thi tro giup\r\n");
	}else{
		USART1_SendString("Lenh khong hop le!Go HELP de xem tro giup\r\n");
	}
}
	
void AttendanceSystem_Config(void){
	USART1_SendString("HE THONG DIEM DANH SINH VIEN\r\n");
	USART1_SendString("San sang doc the RFID...\r\n");
	USART1_SendString("============================\r\n");
	
	FLASH_Config();
	FLASH_ReadRecords();
}

int main(void) {
    uint8_t uid[10];
    uint8_t uidLength;
    int8_t studentIndex;

    volatile uint32_t j; 

    
    SPI1_Config();
    USART1_Config();
    RC522_Config();
		AttendanceSystem_Config();

    
    while (1) {
      
        if (RC522_Check(uid, &uidLength) == MI_OK) {
            
					sprintf(buffer, "UID: %02X:%02X:%02X:%02X:%02X\r\n",
									uid[0], uid[1], uid[2], uid[3], uid[4]);
					USART1_SendString(buffer);
					
					studentIndex = CheckCardRegistered(uid, uidLength);
					MarkAttendance(studentIndex);
					
					ShowAttendanceSummary();
					for(volatile uint32_t j = 0; j < 1000000; j++);
        }
    }
}
            
            
         


