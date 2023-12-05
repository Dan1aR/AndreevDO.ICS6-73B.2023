#define F_CPU 8000000UL

#include <stdio.h>
#include <avr/io.h>
#include <string.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define DHT_PORT PORTC
#define DHT_DDR DDRC
#define DHT_PIN PINC
#define DHT_BIT 4

#define BUT_DDR		DDRC
#define BUT_PORT	PORTC
#define BUT_PIN		PINC
#define BUT_OPEN	0
#define BUT_CLOSE	1
#define BUT_MODE	2

#define DOOR_CLOSED	0
#define DOOR_OPEN	1

#define NO	0
#define YES	1

uint32_t timeSec = 0;
uint8_t boundaryTemperature = 40;
uint8_t boundaryHumidity = 40;
uint8_t doorStatus = DOOR_CLOSED;
uint8_t askSensor = NO;
uint8_t getInfoFromUsart = NO;
uint8_t automaticControl = YES;

uint8_t datadht[5];

const char CMD_SET_HUMIDITY[]	= "h\r";
const char CMD_SET_TEMPERATURE[]= "t\r";	
const char CMD_SET_TIME[]		= "time\r";	
const char CMD_OPEN_DOOR[]		= "o\r";	
const char CMD_CLOSE_DOOR[]		= "c\r";	
const char CMD_GET_INFO[]		= "g\r";	
const char CMD_GET_TIME[]		= "gt\r";	

char data[12]; 
static int usartPutchar(char c, FILE *stream);
static FILE mystdout = FDEV_SETUP_STREAM(usartPutchar, NULL, _FDEV_SETUP_WRITE);


int dhtRead()
{
	for (uint8_t i = 0; i < 5; i++)
	{
		datadht[i] = 0;
	}
	
	DHT_DDR		|=	(1<<DHT_BIT);	
	DHT_PORT	&=~	(1<<DHT_BIT);	
	_delay_ms (18);					
	DHT_PORT	|=	(1<<DHT_BIT);	
	_delay_us (40);					
	
	
	DHT_DDR &=~(1<<DHT_BIT);		
	if (DHT_PIN&(1<<DHT_BIT))		
	{
		return 0;
	}
	_delay_us (80);					
	if (!(DHT_PIN&(1<<DHT_BIT)))	
	{ 
		return 0;
	}

	while (DHT_PIN&(1<<DHT_BIT));
	for (uint8_t j = 0; j < 5; j++)
	{
		datadht[j] = 0;
		
		for (uint8_t i = 0; i < 8; i++)
		{
			cli();								
			while (!(DHT_PIN & (1<<DHT_BIT)));	
			_delay_us(30);						
			if (DHT_PIN & (1<<DHT_BIT))			
			datadht[j] |= 1 << (7 - i);			
			while (DHT_PIN & (1<<DHT_BIT));		
			sei();								
		}
	}
	return 1;
}

void usartInit()
{
	UBRRL=51;							
	UCSRB=(1<<TXEN)|(1<<RXEN);			
	UCSRC=(1<<URSEL)|(3<<UCSZ0);		
	UCSRB |= (1<<RXCIE);				
	
	stdout = &mystdout;
}


static int usartPutchar(char c, FILE *stream)
{
	if (c == '\n')
	usartPutchar('\r', stream);
	while(!(UCSRA & (1<<UDRE)));
	UDR = c;
	return 0;
}


void receivingUsart()
{
	memset(data, 0, sizeof data);
	
	int i=0;
	do {
		while(!(UCSRA&(1<<RXC))) {};
		data[i]=UDR;
		i++;
	} while (data[i-1] != '\r');
}

void setTemperatureUsart()
{
	printf("Enter boundary temperature (20-99): ");
	receivingUsart();
	boundaryTemperature =	(data[0]&0b00001111) * 10 +
							(data[1]&0b00001111);
}

void setHumidityUsart()
{
	printf("Enter boundary humidity (00-99): ");
	receivingUsart();
	boundaryHumidity =	(data[0]&0b00001111) * 10 +
						(data[1]&0b00001111);
}

void moveDoor()
{
	if (doorStatus == DOOR_CLOSED)	PORTD |= (1<<PIND6);
	else							PORTD &= ~(1<<PIND6);
	
	for (uint8_t i = 0; i < 100; i++)
	{
		_delay_ms(10);
		PORTD ^= (1<<PIND7);
	}
}

void getTimeUsart()
{
	uint8_t seconds	= timeSec % 3600 % 60;
	uint8_t minutes = timeSec % 3600 / 60;
	uint8_t hours	= timeSec / 3600;
	
	printf("Time: %d%d:%d%d:%d%d\n", hours / 10, hours % 10, minutes / 10, minutes % 10, seconds / 10, seconds % 10);
}

void openDoorUsart()
{
	if (doorStatus == DOOR_CLOSED)
	{
		if (automaticControl == NO)
		{
			printf("The door opened. ");
			getTimeUsart();
			moveDoor();
			doorStatus = DOOR_OPEN;
		}
		else printf("Turn off automatic control!\n");
		//moveDoor();
		//doorStatus = DOOR_OPEN;
	}
}

void closeDoorUsart()
{
	if (doorStatus == DOOR_OPEN)
	{
		if (automaticControl == NO)
		{
			printf("The door closed. ");
			getTimeUsart();
			moveDoor();
			doorStatus = DOOR_CLOSED;
		}
		else printf("Turn off automatic control!\n");
		//moveDoor();
		//doorStatus = DOOR_CLOSED;
	}
}

void getInfoUsart()
{
	printf("Humidity: %d\n", datadht[0]);						
	printf("Temperature: %d\n", datadht[2]);					
	printf("Boundary humidity: %d\n", boundaryHumidity);		
	printf("Boundary temperature: %d\n", boundaryTemperature);	
}

void setTimeUsart()
{
	printf("Enter time (hh-mm-ss): ");
	receivingUsart();
	
	timeSec =	((data[0] & 0b00001111) * 10 * 3600L)	+
				((data[1] & 0b00001111) * 3600L)		+
				((data[3] & 0b00001111) * 10 * 60)		+
				((data[4] & 0b00001111) * 60)			+
				((data[6] & 0b00001111) * 10)			+
				(data[7] & 0b00001111);
}

ISR(USART_RXC_vect)
{
	receivingUsart();
	
	if (strcmp (data, CMD_SET_TEMPERATURE)==0)		setTemperatureUsart();
	else if (strcmp (data, CMD_SET_HUMIDITY)==0)	setHumidityUsart();
	else if (strcmp (data, CMD_SET_TIME)==0)		setTimeUsart();
	else if (strcmp (data, CMD_OPEN_DOOR)==0)		openDoorUsart();
	else if (strcmp (data, CMD_CLOSE_DOOR)==0)		closeDoorUsart();
	else if (strcmp (data, CMD_GET_INFO)==0)		getInfoUsart();
	else if (strcmp (data, CMD_GET_TIME)==0)		getTimeUsart();
	else											printf("There is no such command!\n");
}

void timer1Init()
{
	TCCR1B |= (1<<WGM12);	
	TIMSK |= (1<<OCIE1A);	
	
	
	
	TCCR1B |= (1<<CS12);	
	OCR1AH = 0b01111010;	
	OCR1AL = 0b00010010;	
}


ISR(TIMER1_COMPA_vect)
{
	timeSec++;
	askSensor = YES;
	
	if (timeSec % 5 == 0)
	{
		getInfoFromUsart = YES;
	}
	
	if (timeSec == 86400)
	{
		timeSec = 0;
	}
}

void checkingButtons()
{
	if (!(BUT_PIN&(0x01<<BUT_MODE)))
	{
		automaticControl = YES;
	}
	else
	{
		automaticControl = NO;
	}
	
	if (!(BUT_PIN&(0x01<<BUT_OPEN)))
	{
		while (!(BUT_PIN&(0x01<<BUT_OPEN)));
		
		if (doorStatus == DOOR_CLOSED)
		{
			if (automaticControl == NO)
			{
				printf("The door opened. ");
				getTimeUsart();
				moveDoor();
				doorStatus = DOOR_OPEN;
			}
			else printf("Turn off automatic control!\n");
		}
	}
	else if (!(BUT_PIN&(0x01<<BUT_CLOSE)))
	{
		while (!(BUT_PIN&(0x01<<BUT_CLOSE)));
		
		if (doorStatus == DOOR_OPEN)
		{
			if (automaticControl == NO)
			{
				printf("The door closed. ");
				getTimeUsart();
				moveDoor();
				doorStatus = DOOR_CLOSED;
			}
			else printf("Turn off automatic control!\n");
		}
	}
}

void askDHT11()
{
	if (askSensor == YES)
	{
		cli (); 
		dhtRead (); 
		sei ();
		
		if ((datadht[0] > boundaryHumidity) || (datadht[2] > boundaryTemperature))
		{
			if (doorStatus == DOOR_CLOSED)
			{
				if (automaticControl == YES)
				{
					printf("The door opened. ");
					getTimeUsart();
					moveDoor();
					doorStatus = DOOR_OPEN;
				}
			}
		}
		else
		{
			if (doorStatus == DOOR_OPEN)
			{
				if (automaticControl == YES)
				{
					printf("The door closed. ");
					getTimeUsart();
					moveDoor();
					doorStatus = DOOR_CLOSED;
				}
			}
		}
		
		askSensor = NO;
	}
}

void getInfoEvery5sec()
{
	if (getInfoFromUsart == YES)
	{
		getInfoUsart();
		getInfoFromUsart = NO;
	}
}

void portInit()
{
	BUT_DDR=0x00;
	BUT_PORT=0x0f;
	
	DDRD = 0xf0;
	PORTD = 0x00;
}

int main(void)
{
	portInit();
	usartInit();
	timer1Init();
	
	sei();

    while (1) 
    {
		checkingButtons();
		askDHT11();
		getInfoEvery5sec();
    }
}

