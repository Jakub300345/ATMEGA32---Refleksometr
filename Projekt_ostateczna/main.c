#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 16000000UL
#include <util/delay.h>

//zmienne
#define RS 0
#define RW 1
#define E 2
#define LED 3

#define diodyDDR DDRA
#define diody PORTA

#define przyciskDDR DDRB
#define przycisk PORTB
#define przyciskIN PINB

uint8_t dioda = 0;
uint8_t stop = 1;
uint8_t pudlo = 0;
uint8_t stan_odstepu = 0;
uint8_t resultTab[] = {0,0,0,0};
volatile uint16_t czas[10] = {0,0,0,0,0,0,0,0,0,0};
volatile uint8_t startx = 0;
volatile uint8_t probka = 0;
volatile uint8_t stan = 0;
volatile uint8_t przyciski[8] = {0,0,0,0,0,0,0,0};
volatile uint8_t stan_licznika = 0;
volatile uint16_t losowe [3] = {1000, 2000, 4000};

//funkcje
void TWI_Start() {
	TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
}

void TWI_Stop() {
	TWCR = (1<<TWINT) | (1<<TWSTO) | (1<<TWEN);
	while((TWCR & (1<<TWSTO)));
}

void TWI_Write(uint8_t data) {
	TWDR = data;
	TWCR = (1<<TWINT) | (1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
}

void TWI_Init() {
	TWBR = 92;
	TWCR = (1<<TWEA);
}

void LCD_Write(uint8_t dane, uint8_t rs)
{
	uint8_t co;
	TWI_Start();
	TWI_Write(0x4e);
	co = 0;
	co |= (rs << RS) | (1 << LED) | (1 << E) | (dane & 0xf0);
	
	TWI_Write(co);
	co &=~(1<<E);
	TWI_Write(co);
	
	co = 0;
	co |= (rs<<RS) | (1<<LED) | (1<<E) | ((dane<<4) & 0xf0);
	TWI_Write(co);
	co &= ~(1<<E);
	TWI_Write(co);
	TWI_Stop();
}

void LCD_Init ()
{
	LCD_Write(0x33,0);
	LCD_Write(0x32,0);
	LCD_Write(0x28,0);
	LCD_Write(0x06,0);
	LCD_Write(0x0c,0);
	LCD_Write(0x01,0);
	_delay_ms(2);
}

void LCD_Write_full_string(char* tab) // Pisanie ca³ego napisu
{
	uint8_t i=0;
	
	while(tab[i])
	{
		LCD_Write(tab[i++],1); // zapis symbolu
		if (i==15) // przejscie do nowej 16x2
		{
			LCD_Write(0xc0,0); // \n
		}
	}
	LCD_Write(0x0c,0); // cursor off
}

void newLineInit(void)
{
	LCD_Write(0xC0, 0);
}

uint16_t getTimeSum() // sumowanie czasów klikniêæ
{
	uint16_t time = 0;
	for(uint8_t i = 0; i < 10; i++)
	time += czas[i];
	return time;
}

void mistakesToBCD(uint8_t* tab) // przekszta³canie liczby b³êdów na BCD
{
	tab[0] = pudlo / 10;
	tab[1] = pudlo % 10;
}

void getBCDResult(uint8_t* tabResult, uint16_t result) // przekszta³canie w BCD
{
	tabResult[0] = result / 1000;
	tabResult[1] = (result / 100) % 10;
	tabResult[2] = (result % 100)/10;
	tabResult[3] = (result % 100) % 10;
}

void showResultsOnLabel(void) // wypisanie ca³ego wyniku
{
	LCD_Write(0x01, 0); // czyszczenie ekranu
	uint16_t sum = 0;

	sum = getTimeSum();
	
	_delay_ms(100);
	getBCDResult(resultTab, sum);
	LCD_Write_full_string("SUMA:");
	LCD_Write(resultTab[0]+'0',1);
	LCD_Write('.',1);
	LCD_Write(resultTab[1]+'0',1);
	LCD_Write(resultTab[2]+'0',1);
	LCD_Write(resultTab[3]+'0',1);
	LCD_Write_full_string("s B:");
	mistakesToBCD(resultTab);
	if(resultTab[0] != 0) LCD_Write(resultTab[0]+48,1);
	else LCD_Write(' ',1);
	LCD_Write(resultTab[1]+48,1);
	
	newLineInit();
	LCD_Write_full_string("AVG:");
	getBCDResult(resultTab, sum);
	LCD_Write(resultTab[0]+'0',1);
	LCD_Write(resultTab[1]+'0',1);
	LCD_Write(resultTab[2]+'0',1);
	LCD_Write('.',1);
	LCD_Write(resultTab[3]+'0',1);
	LCD_Write_full_string("ms");
}

void reset_program(void) // reset programu
{
	for(uint8_t i = 0; i < 8  ; i++)
	{
		czas[i] = 0;
		przyciski[i] = 0;
	}
	czas[8] = 0; czas[9] = 0;
	startx = 0;
	stan = 0;
	probka = 0;
	pudlo = 0;
	
	stop = 1;
	
	for (uint8_t i = 0; i < 4; i++)
	{
		resultTab[i] = 0;
	}
}

//program g³ówny
ISR(TIMER0_COMP_vect)
{
	if (stan_licznika ++== 7) // ci¹g³e liczenie do 7
	{
		stan_licznika = 0;
	}
	
	static uint16_t m_s = 0;
	
	if (startx == 2)
	{
		for (uint8_t i = 0; i < 2; i++)
		{
			if (m_s ++== losowe[i]) // losowa iloœæ czekania (losowy odstêp czasu pomiêdzy zapaleniami diód)
			{
				startx = 1;
				m_s = 0;
			}
		}
	}
	
	if (startx == 3)
	{
		czas[probka]++; // liczenie
		for (uint8_t i = 0; i < 8; i++)
		{
			if (!(przyciskIN & (1<<i))) // jeœli trafiony
			{
				switch (przyciski[i])
				{
					case 0:
					przyciski[i] = 1;
					
					break;
					case 1:
					przyciski[i] = 2; // eliminacja drgania styków, przyciski[i] = 2 oznacza ¿e s¹ wciœniête
					
					break;
				}
			}
			else
			{
				switch (przyciski[i]) // eliminacja drgania styków
				{
					case 3: przyciski[i] = 4; break;
					case 4: przyciski[i] = 0; break;
				}
			}
		}
	}
}

ISR(TIMER1_COMPA_vect)
{
	static uint8_t liczba = 0;
	
	switch (stan)
	{
		case 0:
		if (przyciskIN != 0xff) // jeœli jakikolwiek wciœniêty przycisk
		{
			if (liczba ++== 200) // trzeba przytrzymaæ przycisk
			{
				stan = 1;
				startx = 2; // przejœcie do czekania, a przy startx 1 (w if w liczniku 1) losowana jest dioda ni¿ej
				liczba = 0;
			}
		}
		
		break;
		
		case 1:
		if (probka == 10)
		{
			stan = 3;
		}
		break;
		
		case 3:
		showResultsOnLabel();
		reset_program();
		
		_delay_ms(1000);
		break;
	}
}

int main(void)
{
	diodyDDR = 0xff;
	diody = 0xff;
	przyciskDDR = 0x00; //ustawienie pina wejsciowego input
	przycisk = 0xff;
	TCCR0 = (1<<CS02)|(1<<WGM01); //preskaler 64 i przerwanie gdy OCR0
	TCCR1B = (1<<CS10) | (1<<WGM12); // bez preskaler
	OCR1A = 16000; // 1 ms
	OCR0 = 250; // 1 ms
	
	TWI_Init();
	LCD_Init ();

	TIMSK |= (1<<OCIE1A) | (1<<OCIE0);
	sei();
	
	while (1)
	{
		switch(stan)
		{
			case 0:
			
			break;
			case 1:
			if (startx == 1) // losowanie diody
			{
				dioda = stan_licznika;
				diody = ~(1<<dioda);
				startx = 3;  //zaczyna odliczanie
				break;
			}
			if (startx == 3)
			{
				for (uint8_t i = 0; i < 8; i++)
				{
					if (przyciski[i] == 2) // stan 2 na przyciskach oznacza ¿e s¹ wciœniête
					{
						if (i == dioda) // jeœli trafiony
						{
							diody = 0xff;
							if (probka ++== 10) // robi 10 razy
							{
								stan = 3;
							}
							else
							startx = 2;
						}
						else
						pudlo++; // zliczanie b³êdów
						przyciski[i] = 3;
					}
				}
			}
			break;
		}
	}
}