#include "MDR1986VK01.h"
#include "MDR1986VK01_clk.h"
#include "MDR1986VK01_port.h"
#include "stdio.h"

#define LedGreen PORT_Pin_17
#define LedBlue PORT_Pin_19
#define LedRed PORT_Pin_21

#define ImpulseOut PORT_Pin_13
#define ImpulseIn PORT_Pin_12
#define ButtonUp PORT_Pin_28

void CAP0_IRQHandler(void);


void vError(void)
{
	PORT_SetBits(MDR_PORTC, LedRed);
	while(1);
}

void Delay(uint32_t waitTicks)
{
	uint32_t i;
	for (i = 0; i < waitTicks; i++);
}


int main(void)
{
	PORT_InitTypeDef Port;

	NVIC_EnableIRQ(CAP0_IRQn); // Разрешение прерываний блока CAP0
//	NVIC_EnableIRQ(CAP1_IRQn);
//	NVIC_EnableIRQ(CAP2_IRQn);
//	NVIC_EnableIRQ(CAP3_IRQn);


  MDR_CLK->KEY = 0x8555AAA1;
  MDR_CLK->PER0_CLK =  0xFFFFFFFF;
  MDR_CLK->PER1_CLK =  0xFFFFFFFF;

	// Конфигурация светодиодов
  MDR_PORTC->KEY = 0x8555AAA1;
	PORT_DeInit(MDR_PORTC);
  PORT_StructInit(&Port);
  Port.PORT_Pin     = LedGreen | LedBlue | LedRed;
  Port.PORT_SOE     = PORT_SOE_OUT;
	Port.PORT_SFUNC   = PORT_SFUNC_PORT;
	Port.PORT_SANALOG = PORT_SANALOG_DIGITAL;
  Port.PORT_SPWR    = PORT_SPWR_300;
  PORT_Init(MDR_PORTC, &Port);

	// Конфигурация кнопки Up (S7)
	Port.PORT_Pin = ButtonUp;
	Port.PORT_SOE = PORT_SOE_IN;
  PORT_Init(MDR_PORTC, &Port);

	// Конфигурация вывода, куда будет передаваться уровень при нажатии кнопки
	MDR_PORTB->KEY = 0x8555AAA1;
	PORT_DeInit(MDR_PORTB);
	PORT_StructInit(&Port);
	Port.PORT_Pin = ImpulseOut;
	Port.PORT_SOE     = PORT_SOE_OUT;
	Port.PORT_SFUNC   = PORT_SFUNC_PORT;
	Port.PORT_SANALOG = PORT_SANALOG_DIGITAL;
  Port.PORT_SPWR    = PORT_SPWR_300;
	PORT_Init(MDR_PORTB, &Port);

	// Конфигурация вывода блока контроллера захвата CAP0 для режима захвата (Capture)
	Port.PORT_Pin = ImpulseIn;
	Port.PORT_SOE     = PORT_SOE_IN;
	Port.PORT_SFUNC   = PORT_SFUNC_13;
	PORT_Init(MDR_PORTB, &Port);

	// Конфигурация вывода блока контроллера захвата CAP3 для режима ШИМ (PWM)
	MDR_PORTA->KEY = 0x8555AAA1;
  Port.PORT_Pin     = PORT_Pin_24;
  Port.PORT_SOE     = PORT_SOE_OUT;
  Port.PORT_SFUNC   = PORT_SFUNC_13;
  Port.PORT_SANALOG = PORT_SANALOG_DIGITAL;
  Port.PORT_SPWR    = PORT_SPWR_10;
  PORT_Init(MDR_PORTA, &Port);


	// Настройка тактирования от HSE
	MDR_COMP0->ANABG_CTRL=0x00000081;
  MDR_BKP->KEY = 0x8555AAA1;
	CLK_XSEConfig(CLK_HSE0, CLK_XSE_ON);

	if(!CLK_XSEWaitReady(CLK_HSE0))
	vError();
	MDR_CLK->MAX_CLK = MAX_CLK_HSE0;

	//  Строки кода, отвечающие за работу блока захвата в режиме захвата на выводе PB12 //
	MDR_CLK -> CAP0_CLK |= (1 << 16); // Дополнительная настройка тактирования блока CAP0
  MDR_CAP0 -> ECCTL1 |= (1 << 8); // Настройка бита CAPLDEN. Разрешение загрузки регистров захвата CAP0-4 внутри блока CAP0

	//MDR_CAP0 -> ECCTL1 |= (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6); // Перенастройка регистров CAP1...4POL для захвата по срезу, а не фронту

	MDR_CAP0 -> ECCTL2 = 0; // Бит TSCTRSTOP по умолчанию в единице, бит отвечает за работу таймера, считающего в регистре TSCTR. Для включения нужно перевести в ноль
	MDR_CAP0 -> ECEINT |= (1 << 4); // Разрешение прерывания по событию CEVT4, то есть, по записи в регистр CAP4

	//  Строки кода, отвечающие за работу блока захвата в режим ШИМ (PWM) на выводе PA24 //
	MDR_CLK -> CAP3_CLK |= (1 << 16) | 0; // Дополнительная настройка тактирования блока CAP3
	MDR_CAP3 -> ECCTL2 = 1 << 9; // Настройка блока захвата CAP3 в режим ШИМ

  MDR_CAP3 -> CAP1 = 100; // Период
  MDR_CAP3 -> CAP2 = 20; // Значение сравнения для вывода ШИМ
  MDR_CAP3 -> CAP3 = 100; // Дублирование периода
  MDR_CAP3 -> CAP4 = 20; // Дублирование значения сравнения для вывода ШИМ

	//MDR_CAP3 -> ECCTL2 |= (1 << 10); // Перенастройка полярности сигнала на выводе ШИМ. При единице в десятом бите APWMPOL активным уровнем будет низкий

	// Строки кода, отвечающие за синхронизацию. Снять комментирование для использования//

//	MDR_CAP3->TSCTR   =0x00000000; // Обнуление значения счётчика
//  MDR_CAP3->CTRPHS  =0x0000010; // Задание фазы
//	MDR_CAP3->ECCTL1  = (1<<16); // Сигнал пересинхронизируется на частоту работы блока
//	MDR_CAP3->ECCTL2  = (1<<5) | (1<<6); // Пятый бит SYNCI_EN разрешает загрузку регистра TSCTR значением регистра CTRPHS, шестой бит позволяет генерировать событие синхронизации по достижении периода счёта
//	MDR_CAP3->ECEINT  = 0; // Запрет прерываний
//  MDR_CAP3->ECFLG   = 0; // Очистка флагов прерываний
//  MDR_CAP3->ECCLR   = 0;


	while (1)
	{
		//MDR_CAP3->ECCTL2 = (1<<8) // Программное управление синхронизацией основного счётчика TSCTR. Снять комментарий для программного события синхронизации
		if (PORT_ReadInputDataBit(MDR_PORTC, ButtonUp) == Bit_RESET)
		{
		PORT_SetBits(MDR_PORTB, ImpulseOut);
		Delay(500000);
		PORT_ResetBits(MDR_PORTB, ImpulseOut);
		}
	}

}
// Обработчик прерывания блока захвата //
void CAP0_IRQHandler()
{
	PORT_SetBits(MDR_PORTC, LedRed);
	PORT_SetBits(MDR_PORTC, LedGreen);
	PORT_SetBits(MDR_PORTC, LedBlue);
	MDR_CAP0 -> ECCLR |= (1 << 4); // Очистка прерывания CAP0
	Delay(1000000);
	PORT_ResetBits(MDR_PORTC, LedRed);
	PORT_ResetBits(MDR_PORTC, LedGreen);
	PORT_ResetBits(MDR_PORTC, LedBlue);
}
