#define STM8L15X_MD
#include "StdPeriph\inc\stm8l15x.h"
typedef struct segment_
{
  unsigned int len;
  int step;
  unsigned int leap_dec;
  unsigned int leap_inc;
  char flags;
} segment_t;

__tiny unsigned int dac[100];
__tiny char armed;
__tiny unsigned int trg_count;
char cmd[23];
char cmd_ptr;
char dg[] = "0123456789ABCDEF";
char uid[] = "001122334455";
segment_t function[50];

extern void dds_init(segment_t*);
extern void prepare(unsigned int *);
extern unsigned int lookup[];
unsigned int preheat;
char digit(char h)
{
	char i;
	for(i = 0; i < 16; i++)
	{
		if(dg[i] == h)
			return i;
	}
	return 100;
}
unsigned int hex(char* f, char l)
{
	char i;
	char d;
	unsigned int r = 0;
	for(i = 0; i <l; i++)
	{
		d = digit(*(f+i));
	  	if(d == 100)
			return 0x7FFF;
		r <<= 4;
		r += d;
	}
	return r;
}
void disarm()
{
	if(armed)
	{
		GPIOD->CR2 = 0;     // не ожидаем прерывания от зеркала
		TIM1->CR1 &= ~TIM1_CR1_CEN;
		TIM4->CR1 &= ~TIM1_CR1_CEN;  //выключаем ЦАП
		GPIOB->ODR &= ~(1 << 5);     //выключаем лазер
		DAC->CH1CR1 &= ~DAC_CR1_TEN; //пишем в ЦАП ноль
		DAC->CH1RDHRH = 0;
		DAC->CH1RDHRL = 0;
		DAC->CH1CR1 |= DAC_CR1_TEN;
		armed = 0;
	}
}
void arm()
{
	int i;
	unsigned long j = 0;
	if(armed == 0)
	{
		for(i = 0; i <50; i++)
		{
			j+=function[i].len;
			if(function[i].flags & 0x01)
			{
				break;
			}
		}
		j+=2;
		trg_count = j >> 16;
		j = j % 0x10000;
		j = 0x10000 - j;
		TIM1->CNTRH = j >> 8;
		TIM1->CNTRL = j % 0x100;
		TIM1->CR1 |= TIM1_CR1_CEN;
		TIM4->CNTR = TIM4->ARR -1;
		dds_init(function);
		prepare(dac);
		prepare(dac+50);
		DMA1_Channel3->CCR &= ~DMA_CCR_CE; 
		DMA1_Channel3->CNBTR = 100;
		DMA1_Channel3->CCR |= DMA_CCR_CE; 
		armed = 1;
		GPIOD->CR2 = 1;     //ожидаем прерывания от зеркала
	}
	return;
}
void parse()
{
	unsigned int t;
	segment_t seg;
  	switch(cmd[0])
	{
	case 'A':                                        //Добавление точки
		t = hex(cmd+1, 2);
		if(t == 0x7FFF)
		  break;
		seg.len = hex(cmd+3,4);
		if(seg.len == 0x7FFF)
		  break;
		seg.step = hex(cmd+7,4);
		if(seg.step == 0x7FFF)
		  break;
		seg.leap_dec = hex(cmd+11,4);
		if(seg.leap_dec == 0x7FFF)
		  break;
		seg.leap_inc = hex(cmd+15,4);
		if(seg.leap_inc == 0x7FFF)
		  break;
		seg.flags = hex(cmd+19,2);
		if(hex(cmd+19,2) == 0x7FFF)
		  break;
		if(armed == 0)
		{
			function[t] = seg;
			USART1->DR = '!';
			return;
		}
	case 'P':                                      //Дежурный режим(ARM)
	  	arm();
		USART1->DR = '!';
		return;  
	case 'M':                                      //Релюха:
	  	if(cmd[1] == 'A')                      // A = Automatic
		{
			GPIOB->ODR &= ~0x4;
		  	USART1->DR = '!';
			return;  
		}
	  	if(cmd[1] == 'M')                      // M = Manual
		{
			GPIOB->ODR |= 0x4;
			USART1->DR = '!';
			return;  
		}
		break;
	case 'F':                                      //Формирование импульса(FIRE)
		if(armed)
		{
		  	GPIOD->CR2 = 0;             //перестаем ждать прерывания от зеркала
			GPIOB->ODR |= 1 <<5;        //включаем лазер
			TIM4->CR1 |= TIM1_CR1_CEN;  //включаем ЦАП
		}
		USART1->DR = '!';
		return;
	case 'D':                                     //Отключение (DISARM)
	  	disarm();
		USART1->DR = '!';
		return;  
	case 'V':                                     //Мощность в дежурном режиме
		if(armed == 0)
		{  
			t = hex(cmd+1, 4);
			if(t == 0x7FFF)
			  break;
			preheat = t;
			USART1->DR = '!';
			return;
		}
		break;
	case 'T':                                    //Включение дежурного разряда
	  	if(armed)
		{
	  		t = lookup[preheat];
	  		GPIOB->ODR |= (1 << 5);      //включаем лазер
			DAC->CH1CR1 &= ~DAC_CR1_TEN; //начинаем предподогрев
			DAC->CH1RDHRH = t >> 8;
  			DAC->CH1RDHRL = t %0x100;
			DAC->CH1CR1 |= DAC_CR1_TEN;
		}
		USART1->DR = '!';
		return;
	case 'W':                                      //Получение ID контроллера
  		t = USART1->SR;
	  	USART1->DR = '!';
	  	for(t = 0; t <12; t++)
		{
		  while((USART1->SR & USART_SR_TC) == 0){};
		  	USART1->DR = uid[t];
		}
		return;
	}
	USART1->DR = '?';
	return;
}

#pragma vector=30
__interrupt void usart_rcv(void)
{
	char d;

	d = USART1->SR;
	d = USART1->DR;
	switch(d)
	{
	case 'S':                   //Начало комманды
		cmd_ptr = 0;
		for(d=0;d<23;d++)
		{
			cmd[d] = 0;
		}
		break;
	case 'R':                   //Конец комманды
		parse();
		break;
	default:
		cmd[cmd_ptr] = d;
		cmd_ptr++;
		if(cmd_ptr > 23)
		{
			cmd_ptr = 0;
			for(d=0;d<23;d++)
			{
				cmd[d] = 0;
			}
		}
	}
	return;
}
#pragma vector=25
__interrupt void tim1_ovf(void)
{
	if(trg_count == 0)
	{
		GPIOB->ODR &= ~(1<<5);
		disarm();
	}
	else
	{
		trg_count --;
	}
  	TIM1->SR1=0;
	return;
}
#pragma vector=5
__interrupt void dma_update(void)
{
  	if(DMA1_Channel3->CSPR & DMA_CSPR_HTIF)
	{
		prepare(dac);
		DMA1_Channel3->CSPR &= ~DMA_CSPR_HTIF;
	}
	if(DMA1_Channel3->CSPR & DMA_CSPR_TCIF)
	{
		prepare(dac+50);
		DMA1_Channel3->CSPR &= ~DMA_CSPR_TCIF;
	}
	return;
}
#pragma vector=10
__interrupt void gpioint(void)  //trigger in
{
	GPIOB->ODR |= 1 <<5;
	TIM4->CR1 |= TIM1_CR1_CEN;
	GPIOD->CR2 = 0;
	EXTI->SR1 = 0x01;
	return;
}
int main( void )
{
  char i;
  char * uidp = (char*)0x4926;
  CLK->CKDIVR = 0; // max freq;
  CLK->PCKENR1 = CLK_PCKENR1_DAC | CLK_PCKENR1_USART1 | CLK_PCKENR1_TIM4;
  CLK->PCKENR2 = CLK_PCKENR2_DMA1 | CLK_PCKENR2_COMP | CLK_PCKENR2_TIM1;
  
  DAC->CH1CR1 = DAC_CR1_EN;
  DAC->CH1RDHRH = 0;
  DAC->CH1RDHRL = 0;
  DAC->CH1CR2 = DAC_CR2_DMAEN;
  DAC->CH1CR1 |= DAC_CR1_TEN;
  function[0].len = 40;
  function[0].step = 100;
  function[0].leap_dec = 0;
  function[0].leap_inc = 0;
  function[0].flags = 1;
  preheat = 0;
  dds_init(function);
  prepare(dac);
  prepare(dac+50);
  
  RI->IOSR3 |= 0x10;
  RI->IOCMR3 |= 0x10;
  DMA1->GCSR = 0;
  DMA1_Channel3->CCR = DMA_CCR_TCIE | 
                       DMA_CCR_HTIE | 
                       DMA_CCR_DTD |
                       DMA_CCR_ARM |
                       DMA_CCR_HTIE |
                       DMA_CCR_IDM;
  DMA1_Channel3->CSPR = DMA_CSPR_16BM| DMA_CSPR_PL; //max priority 
  DMA1_Channel3->CPARL = 0x88;     //DAC RIGHT ALIGNED HSB;
  DMA1_Channel3->CPARH = 0x53; 
  
  DMA1_Channel3->CNBTR = 100;
  DMA1_Channel3->CM0EAR = 0;
  DMA1_Channel3->CM0ARH = ((unsigned int)dac)>>8;
  DMA1_Channel3->CM0ARL = ((unsigned int)dac)%0x100;
  DMA1_Channel3->CCR |= DMA_CCR_CE; 
  DMA1->GCSR = DMA_GCSR_GE;
  GPIOB->DDR = 1<<5 | 0x7;
  GPIOB->CR1 = 1<<5 | 0x7;
  GPIOB->CR2 = 1<<5 | 0x7;
  GPIOB->ODR = 0x7;
  GPIOD->DDR = 0;
  GPIOD->CR2 = 0;
  EXTI->CR1 = 0x02;
  ITC->ISPR1 = 0x55;
  ITC->ISPR2 = 0x55;
  ITC->ISPR3 = 0x55;
  ITC->ISPR4 = 0x55;
  ITC->ISPR5 = 0x55;
  ITC->ISPR6 = 0x15;
  ITC->ISPR7 = 0x55;
  ITC->ISPR8 = 0xF5;
//  USART1->BRR2 = 0x35;  //1200
//  USART1->BRR1 = 0x41;
  USART1->BRR2 = 0xD5;  //300
  USART1->BRR1 = 0x05;
  USART1->CR1 = 0;
  USART1->CR2 = USART_CR2_RIEN | USART_CR2_REN | USART_CR2_TEN;
  USART1->CR3 = 0;
  
  
  TIM4->CR1 = TIM1_CR1_URS;
  TIM4->CR2 = 0x20; // trigger on update
  TIM4->SMCR = 0;
  TIM4->ARR = 0x50;
  
  TIM1->SMCR = 0x07; //counts DAC TRGs
  TIM1->IER = TIM1_IER_UIE;
  TIM1->PSCRH = 0;
  TIM1->PSCRL = 0;
  TIM1->CNTRH = 0;
  TIM1->CNTRL = 0;
  for(i = 0; i < 6; i++)
  {
  	uid[i*2] = dg[*(uidp + i) >> 4];
  	uid[i*2 + 1] = dg[*(uidp + i) %16];
  }

  __enable_interrupt();
  while(1);  
}