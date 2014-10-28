// test TRT with two led blinking tasks
// and two UART tasks
#define F_CPU 16000000UL
//our IR funcitons library:
#include "IRLIB.c"
//define semaphores, important to do this before including some of the other libraries
#define SEM_RX_ISR_SIGNAL 1
#define SEM_STRING_DONE 2 // user hit <enter>
//More library includes
#include "trtSettings.h"
#include "trtkernel_1284.c"
#include "trtUart_usart_1.c"
#include <util/delay.h>
#include <stdio.h>
//uart setup getchar and putchar are from the trtUart_usart_1.c file
FILE uart_str = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);
#define time 1200000
// unused leftover semaphore from example code
#define SEM_TASK1_WAIT 3

// two semaphores to protect message --
// sender must wait until message is received 
// -- init to 1 becuase don't need to wait for first message
// receiver must wait until message is sent
#define SEM_TX_WAIT  4
#define SEM_RX_WAIT  5

// unused leftover semaphore
#define SEM_SHARED 7

// the usual
#define begin {
#define end }
#define PORTDIR DDRC
#define PORTDATA PORTC
#define PORTIN PINC

// input arguments to each thread
// not actually used
int args[3] ;


//ISR timer 0 compare match vector
//provided by Bruce "The Boss" Land himself
ISR (TIMER0_COMPA_vect) 
begin  
    unsigned char c ;
        //**********************
      // send an ir char if tx is ready and still char in buffer to send
    // and USART is ready
    if (ir_tx_ready ){ //&& ir_tx_buffer[ir_tx_count]>0
        if (UCSR0A & (1<<UDRE0)) UDR0 = ir_tx_buffer[ir_tx_count++];
        if (ir_tx_buffer[ir_tx_count]==0x00) ir_tx_ready = 0 ; // end of buffer
        if (ir_tx_count >= buffer_size) ir_tx_ready = 0; // buffer overrun
    }
    //**********************
      // recv an ir char if data ready  
      // otherwise set c to null 
    if (UCSR0A & (1<<RXC0) ) {
        c = UDR0 ; // valid char 
    }
    else c = 0 ; // nonvalid

    //**********************
    // append character to the received string
    // if character is valid and we expect a string
    if (c>0) { //&& (ir_rx_ready==0)) {

        if (c == start_token) { // restart the string
            ir_rx_count = 0 ;
        }

        else if (c == end_token){ // end the string
            ir_rx_buffer[ir_rx_count] = 0x00 ;
            ir_rx_ready = 1 ;
        }

        else { // add to string and check for buffer overrun
            ir_rx_buffer[ir_rx_count++] = c ;
            if (ir_rx_count >= buffer_size) { // buffer overrun
                ir_rx_ready = 2;
                ir_rx_buffer[buffer_size-1] = 0x00 ;
                ir_rx_count = buffer_size -1 ; //???
            }
        }

    } // end if c>0
    
end  

// The Lock Task
 void lock(void* args) 
 {
	//variable declaration for the lock task
    char keyid='K';
    char keyidl='L';
    char keysigl;
    char modeal;
    char modebl;
    char modecl;
    char packetl;
    uint32_t rell, deadl ;
    int statel=0;//States: (0|Idle)(1|Challenge)(2|Response)
    char datatoget[16];
    char thymel[10];
    char responsedata[16];

    while(1)
    {
    //fprintf(stdout,"lock"); //added this for debugging
    keysigl=PINB;
	//define three modes
    modeal=keysigl&0b00000001;
    modebl=keysigl&0b00000010;
    modecl=keysigl&0b00000100;
	//Check modes and print what mode it is in
    if(keysigl==250){
        fprintf(stdout, "Test Mode!\n");
    }
    if(keysigl==253||keysigl==251||keysigl==249)//is the lock on?
    {
    if(keysigl==249)
    {
        fprintf(stdout, "Standard Lock Mode:\n");
    }

	//Define Idle State (State 0)
        if(statel==0)
        {
		//Wait for the receive semaphore to give the go-ahead.
            trtWait(SEM_RX_WAIT);
			//Receive the packet using the receive packet function from IRLIB
            packetl=ir_rec_packet(keyid,datatoget[16]);
			//reset the Semaphore to signal that we are done receiving
            trtSignal(SEM_RX_WAIT);
            //if packet==0, valid data is received
			if(!packetl)
            {
			//check the identifier to ensure that it's a valid request
                if(datatoget[0]=='r')
                {
                    //if it is a valid request, proceed to challenge state
                    statel=1;
                }
            }
        }
        else if(statel==1)//challenge
        {
		//record the current time in variable thymel
            sprintf(thymel,"%ld",trtCurrentTime());
			//Wait for the transmission semaphore
            trtWait(SEM_TX_WAIT);
			//Send back the key identifier symbol and the current time
            ir_send_packet(keyidl,thymel);
			//signal the transmission semaphore
            trtSignal(SEM_TX_WAIT);
			//proceed to the next state (response)
            statel=2;
        }
		//response state
        else if(statel==2)
        {
		//wait for the receiving semaphore
        trtWait(SEM_RX_WAIT);
		//receive the message
        packetl=ir_rec_packet(keyid,responsedata);
		//signal the receiving semaphore to free the receiving protocol
        trtSignal(SEM_RX_WAIT);
        //declare a few variables that are about to be used
        char tagstr[16] = "";
        int k;
        int n = strlen(responsedata);
        char temp;
		//for loop to concatenate the response together
        for (k=2; k < n; k++) 
        {
		//temporary variable to store string in
            temp = responsedata[k];
			sprintf(temp, "%c", responsedata[k]);
            strcat(tagstr, temp);
        }
		//if the packet == 0, then it is a valid packet
            if(!packetl)
            {
			//check the identifier of the response data for validity
                if(responsedata[1]=='>')
                {
				//turn the tagstring and thymel into integers and check that their
				//difference is less than one second
                    if((atoi(tagstr)-atoi(thymel))<SECONDS2TICKS(1))
                    {
					//check whether we are locking or unlocking
                        if(responsedata[0]=='0')
                        {
						//unlock
                            PORTC = PORTC | 0x01 ;
                        }
                        else if(responsedata[0]=='1')
                        {
						//lock
                            PORTC = PORTC & 0xfe ;
                        }//
                    }//end timecheck if
                }//end validation if
            }//end packetif
        }
    }
    
    
    
		//set release time to .1 seconds from now
        rell = trtCurrentTime() + SECONDS2TICKS(0.1);
		//set dead time to .2 seconds from now
        deadl = trtCurrentTime() + SECONDS2TICKS(0.2);
		//sleep based on the given release and dead times
        trtSleepUntil(rell, deadl);
    

    }//end lockon

}

// key task
void key(void* args) 
  {
  //variable declarations for the key task
      uint32_t rel, dead ;
    char keysig;
    char modea;
    char modeb;
    char modec;
    char butts;
    char keyval;
    char keyid='K';//K for key
    char keyidl='L';//L for lock
    char r='r';
    char packet;
    char buttsl="w";
    char command=2;
    char shit[20];
    char irdatarec[20];
    int state=0;//States: (0|Idle)(1|Request)(2|Challenge)(3|Response)
    char thyme[20];
    char datatosend[16];
    char ir_tx_data[20];
    butts=PINC&0b00000110;
    while(1)

    {
	//check the SPDT switches on port B to determine the mode
    keysig=PINB;
    modea=keysig&0b00000001;
    modeb=keysig&0b00000010;
    modec=keysig&0b00000100;
	//save the last state of buttons
    buttsl=butts;
	//make the new state of buttons
    butts=PINC;
	//checking which mode it is operating in, and then sending that mode to PuTTY
        if(keysig==253||keysig==251||keysig==255)//Either test mode or normal as key mode
        {
        if (keysig == 255)
        {
            fprintf(stdout, "Standard Key Mode: Unauthenticated\n");
        }
        if (keysig == 251)
        {
            fprintf(stdout, "Standard Key Mode:Authenticated\n");
        }
        if (butts == 117) {
            fprintf(stdout, "Send Lock Attempt\n");
        }
        if (butts == 47 || butts == 111) {
            fprintf(stdout, "Send Unlock Attempt\n");
        }
		//if butts is different, change the state from idle to 1
            if(butts!=buttsl)
            
            {
                //fprintf(stdout,"K0\n\r");
                state=1;
            }
            //if the state is 1 (request) send a request
            if(state==1)//request
            {
			//format 'r' into the data to be sent
                sprintf(ir_tx_data,'r');
				//wait for the transmission semaphore to be freed
                trtWait(SEM_TX_WAIT);
				//send the data over IR
                ir_send_packet(keyid,ir_tx_data);
				//signal that you are done sending
                trtSignal(SEM_TX_WAIT);
				//progress to the challenge state
                state=2;
                
            }
			//challenge state
            else if(state==2)
            {
			//wait for the receive semaphore
                trtWait(SEM_RX_WAIT);
				//receive the packet from IR
                packet=ir_rec_packet(keyidl,irdatarec);
				//signal the receive semaphore to signal that you are done receiving
                trtSignal(SEM_RX_WAIT);
				//if the packet is valid
                if(packet==0)
                {
				//progress to hte response state
                    state=3;
					//save the time received from the lock for later use
                    strncpy(thyme,irdatarec,20);
                }
            }
			//response state
            else if(state==3)
            {

                //fprintf(stdout,"K3\n\r");
            //formatting lock command
                if(butts==0b00000100)
                {
                    command='0';//command is ASCII '0' for unlock
                }
                else if(butts==0b00000010)
                {
                    command='1';//command is ASCII '1' for lock
                }            
                //formatting valid/invalid
                if(keysig==253)//checking if it's a valid keyId or not
                {
                    keyid='<';// "<" invalid
                }
                else if(keysig==249)
                {
                    keyid='>';// ">" valid
                }
				//format the entire data string to send
                sprintf(datatosend,"%c%c%s",command,keyid,thyme);
				//wait for the transmit semaphore
                trtWait(SEM_TX_WAIT);
				//send the packet
                ir_send_packet(keyid,datatosend);
				//signal the transmit semaphore to let TRT know that you are done sending
                trtSignal(SEM_TX_WAIT);
				//reset the state to idle
                state=0;
            }
        }//end KEYON
		//set release to .1 seconds from now
    rel = trtCurrentTime() + SECONDS2TICKS(0.1);
	//set dead time to .2 seconds from now
    dead = trtCurrentTime() + SECONDS2TICKS(0.2);
	//sleep given these parameters
    trtSleepUntil(rel, dead);
    }

}    
    
        
        
        
        
        
        




// --- Main Program ----------------------------------
void main(void) {
//irinitialize function from the IRLIB
  irinitialize();
  //Setting Pin C direction so that C0 is an LED and C1 and C2 are buttons
  DDRC = 0x01;   
  //set LED high, all inputs have no pullups
  PORTC = 0x01;
  //Setting port B directions
  DDRB = 0xf8; 
  //defining pullup state as on for port b
  PORTB = 0xff; 
  //initiate the TRT uart
  trt_uart_init();
  stdout = stdin = stderr = &uart_str;
  
  //fprintf(stdout,"\n\r%d\n\r",PINB);
  fprintf(stdout,"\n\r...Starting IR comm ...\n\r");
  // start TRT Kernel
  trtInitKernel(80); // 80 bytes for the idle task stack

  // Semaphores for Uart
  trtCreateSemaphore(SEM_RX_ISR_SIGNAL, 0) ; // uart receive ISR semaphore
  trtCreateSemaphore(SEM_STRING_DONE,0) ;  // user typed <enter>

  // unused semaphore left in from example code we edited
  trtCreateSemaphore(SEM_TASK1_WAIT, 0) ; // task2 controls task1 rate
  
  // message protection is used to ensure that only one receive or transmit
  //occurs at a time
  trtCreateSemaphore(SEM_TX_WAIT, 1) ; // message send interlock
  trtCreateSemaphore(SEM_RX_WAIT, 1) ; // message receive interlock
  
  // unused semapore left in from example code we edited
  trtCreateSemaphore(SEM_SHARED, 1) ; // protect shared variable

 // Creating tasks, we gave them ample stack size and execute time to ensure that they would function properly
  trtCreateTask(key, 400, SECONDS2TICKS(0.1), SECONDS2TICKS(0.5), &(args[1]));
  trtCreateTask(lock, 400, SECONDS2TICKS(0.1), SECONDS2TICKS(0.5), &(args[0]));
 
  // Infinite with noting in it, this relinquishes all control to the ISRs and TRT
  while (1) 
  {
      //fprintf(stdout, "main loop");//added 
    //PORTC = 0x01;//FOR DEBUGGING
  }

} // main