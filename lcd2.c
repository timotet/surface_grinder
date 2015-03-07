 /*
   
   surface grinder timer control
   
   Program for RTC and timed relay operation for controlling a 12vdc 500ma surface grinder 
   3 button interface to set time and control the timer
   display of clock and timer info on cheap wintek 24x1 LCD
   
   UC runs at 1.00Mhz RTC uses 32 Khz crystal for timer and time keeping
   uses timerA for  timer operation and WDT for clock operation
   
   32khz watch crystal on pins 13 and 12
   
   operation;
   once powered on its in clock set mode
   set the clock via 3 buttons hours, minutes, set
   once clock is set its in clock mode waiting for a port1 interrupt
   when a port1 interrupt occurs it goes into timer mode
   if the 1st button was pressed the relay switchs on for 5 seconds times the # of button presses
   if the 2nd button was pressed it goes for minutes based on the # of button presses
   the 3rd button resets the timer and goes back into clock mode
   
   thanks to the following:
   
   everyone on the 43oh forum
   
   and especially:
   
   bluehash
   RobG
   oPossum
   
   
   hex instructions for display setup
   0x1C == LCD power control         PW
   0x14 == Display on/off            DO
   0x28 == Display Control sets lines ect.   DC
   0x4F == Contrast Set              CN
   0xE0 == DDRAM address set         DA
   
   
 */  
    #include "RTC.h"
    #include "msp430g2211.h"
   
   
    #define sendData(data) send(data, 1)
    #define sendInstruction(data) send(data, 0)
    #define clearDisplay() sendInstruction(0x01)
    #define clockSetMode 1
    #define clockMode 0
    #define timerMode 2
    
    //////////////port 1 lcd defines///////////////
    #define DATAPIN BIT0             // p1.0 pin 2 on msp430
    #define CLOCKPIN BIT1            // p1.1 pin 3
    #define ENABLEPIN BIT2           // p1.2 pin 4
    #define RESETPIN BIT3            // p1.3 pin 5
    
    /////////// port 1 button defines/////////////
    #define b_1 BIT5              // p1.5 pin 7 on msp430   seconds ++ for timer / minutes ++ for setting time
    #define b_2 BIT6              // p1.6 pin 8             minutes ++ for timer / hours ++ for setting time
    #define bSet BIT7             // p1.7 pin 9             set for setting time & timer  / reset for relay
    
    //////////port 1 relay define////////////////
    #define relay BIT4            // p1.4 pin 6 on msp430
    
    char mode = clockSetMode;
    char charIndex = 0;
    char bitCounter = 0;
    char b_1Counter = 0;          //counter for button 1     seconds
    char b_1Seconds = 0;
    unsigned int b_2Counter = 0;    //counter for button 2     minutes
    unsigned int secsToCount;      // seconds counter
    char bSetCounter = 0;         //counter for button 3
    
    char ti_secTemp = 0;
    char ti_minTemp = 0;
    char ti_hourTemp = 0;
    char secMin = 0;        //char for secMin variable
    
    char charsToSend[4] = {0,0,0,0};
    char secsToSend[4] = {0,0,0,0};
    const char charMap[10] = {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39}; // numbers to send in hex
    const char message1[12] = {0x20,0x20,0x20,0x73,0x65,0x74,0x20,0x63,0x6c,0x6f,0x63,0x6b};//set clock
    const char message2[12] = {0x20,0x20,0x20,0xA4,0x20,0xAE,0x65,0x73,0x65,0x74,0x20,0xA4};  //reset
    const char message3[9] = {0x54,0x69,0x6D,0x65,0x73,0x20,0x55,0x70,0x21};  //Times Up!
    const char message5[18] = {0x20,0x20,0x20,0x53,0x65,0x74,0x20,0x53,0x65,0x63,0x6f,0x6e,0x64,0x73,0x20,0x20,0x20,0x20}; //set seconds
    const char message6[18] = {0x20,0x20,0x20,0x53,0x65,0x74,0x20,0x4d,0x69,0x6e,0x75,0x74,0x65,0x73,0x20,0x20,0x20,0x20}; //set minutes
      
    unsigned int minutes = 0;               // minutes
    unsigned int seconds = 0;               // seconds
    unsigned int binary = 0;
    
    ///////////////////////functions////////////////////////////////
    
    void delay(unsigned int ms) {    // Delays by the specified Milliseconds
       while (ms--){
     __delay_cycles(1000); // set to 1000 for 1 Mhz
      }
    }

    void send(char data, char registerSelect) {
       bitCounter = 0;
       while(bitCounter < 8) {
          (data & BIT7) ? (P1OUT |= DATAPIN) : (P1OUT &= ~DATAPIN);
          data <<= 1;
          P1OUT |= CLOCKPIN;
          P1OUT &= ~CLOCKPIN;
          bitCounter++;
     }
         registerSelect ? (P1OUT |= DATAPIN) : (P1OUT &= ~DATAPIN);
         P1OUT &= ~ENABLEPIN;
         _delay_cycles(3000);                    
         P1OUT |= ENABLEPIN;
         _delay_cycles(10000);
         P1OUT &= ~ENABLEPIN;
    }
    
     void sendDataArray(const char data[], char length) {
       charIndex = 0;
         while(charIndex < length) {
            sendData(data[charIndex]);
            charIndex++;
         }
    }
    
    void initDisplay(void) {
    	P1OUT &= ~RESETPIN;                     // RESETPIN low
        _delay_cycles(1000);
        P1OUT |= RESETPIN;                      //RESETPIN high
    	sendInstruction(0x1C);             //PW
    	sendInstruction(0x14);             //DO   0x14
    	sendInstruction(0x28);             //DC   0x28
    	sendInstruction(0x4F);             //CN   0x4F
    	sendInstruction(0xE0);             //DA
    	//sendInstruction(0x72);             //SC
    	//sendInstruction(0xC);              //CR
    }
    
    void init_p1(void){
    	P1DIR |= ENABLEPIN | CLOCKPIN | DATAPIN | RESETPIN | relay; // sets ENABLEPIN,CLOCKPIN,DATAPIN,RESETPIN and relay to outputs
        P1OUT &= ~(CLOCKPIN | DATAPIN | RESETPIN | relay);          // sets CLOCKPIN,RESETPIN,and DATAPIN and relay low
        P1OUT |= ENABLEPIN;                                         // sets ENABLEPIN high
        
        P1IN &= ~(b_1 | b_2 | bSet);    // input signal == low
        P1DIR &= ~(b_1 | b_2 | bSet);   // set b_1, b_2, bSet input
        P1OUT &= ~(b_1 | b_2 | bSet);   // pulldown for b_1, b_2, bSet 
        P1REN |= b_1 | b_2 | bSet;      // Resistor Enable for  b_1, b_2, bSet 
        P1IES &= ~(b_1 | b_2 | bSet);   // Interrupt Edge Select - 0: trigger on rising edge, 1: trigger on falling edge
        P1IFG &= ~(b_1 | b_2 | bSet);   // interrupt flag for b_1, b_2, bSet is cleared
        P1IE |= b_1 | b_2 | bSet;       // enable interrupt for b_1, b_2, bSet
    }
  
    
    void CharsToSend(void){
    	charIndex = 0;
        while(charIndex < 4) {
            charsToSend[charIndex] = charMap[0];
            charIndex++;
        }
        
        charIndex = 3;
        while(binary > 0) {
            charsToSend[charIndex] = charMap[binary % 10];
            binary /= 10;
            charIndex--;
           }
    }
    
    void ClockSetMode(void){            //function to set clock
         sendDataArray(message1, 12);    // send message using sendDataArray "set clock"
         _BIS_SR(LPM3_bits + GIE);
     }	
     
    void sendTime(void){                  //function to send time to display
        clearDisplay();    
        ti_hourTemp = (TI_hour >> 4) + '0';
        sendData(ti_hourTemp);
        ti_hourTemp = (TI_hour & 0x0F) + '0';
        sendData(ti_hourTemp);
        sendData(0x3a);
        ti_minTemp = (TI_minute >> 4) + '0';
        sendData(ti_minTemp);
        ti_minTemp = (TI_minute & 0x0F) + '0';
        sendData(ti_minTemp);
        sendData(0x3a);
        ti_secTemp = (TI_second >> 4) + '0';
        sendData(ti_secTemp);
        ti_secTemp = (TI_second & 0x0F) + '0';
        sendData(ti_secTemp);
    
    if(TI_PM == 0){              //sets am or pm to display
        sendData(0x20);	
        sendData(0x41);
        sendData(0x4d);
      }
    else{
        sendData(0x20);	
        sendData(0x50);
        sendData(0x4d);
       }
      }
    
    void timesUp(void){
    	clearDisplay();
       	sendDataArray(message3, 9);    // send message using sendDataArray "times up"
       	delay(500);
     }	

    //////////////////////main////////////////////////
    void main(void)
    {
       WDTCTL = WDTPW + WDTHOLD;                 //stop watchdog
       
       DCOCTL = CALDCO_1MHZ;                    //calibrate DCO to 1 mhz
	   BCSCTL1 = CALBC1_1MHZ;
	    
	   BCSCTL3 |= LFXT1S_0 | XCAP_3;          // Use Crystal as ACLK + 12.5 pF caps
	
       CCTL0 = CCIE;                          // timerA interrupt enable
       IE1 |= WDTIE;                          // Enable WDT interrupt
                                               
       init_p1();                                // set up port 1
       initDisplay();                            // initiate display
       setTime(0x12,0,0,1);                      //set time to 12:00 pm
       ClockSetMode();                           // function to set clock
       
       _BIS_SR(LPM3_bits + GIE);        //go into low power mode wait for port interrupt 
       
      } 
    
//////////////////////////ISR///////////////////////////////////////////
    
    #pragma vector=WDT_VECTOR
    __interrupt void WDT_ISR(void)    // interrupt flag is cleared automatically 
     {  
      if (mode == clockMode){	
         incrementSeconds();
         sendTime(); 
    } 
      else if(mode == timerMode)
   	    incrementSeconds();
      
    }
    
    #pragma vector=PORT1_VECTOR
    __interrupt void PORT1_ISR(void)
    {          
       LPM3_EXIT; 	
       if(mode == clockSetMode){
       	
       	if (P1IFG & BIT5){
       	   P1IFG &= ~b_1;                       // reset interrupt flag
       	   incrementHours();                    // TI_hours ++
     	}
       	
       	if (P1IFG & BIT6){	
       	   P1IFG &= ~b_2;                        // reset interrupt flag
       	   incrementMinutes();                   // TI_minutes ++ 
       	}
       	 
       	if (P1IFG & BIT7){	                //set time sequence
       	   P1IFG &= ~bSet;                  //clear interrupt  
       	   setTime(TI_hour,TI_minute,0,TI_PM);  //put new values into clock
       	   WDTCTL = WDT_ADLY_1000;          //watch dog interval timer mode 1 sec interrupt for clock
       	   mode = clockMode;
        }
        
      sendTime();
  
     }
     else{                        // timer routines  	
           mode = timerMode;
       if (P1IFG & BIT5){
       	   P1IFG &= ~b_1;  
       	   b_1Counter++;            
       	   b_1Seconds++;
           secsToCount = b_1Counter * 5; //# of seconds timer will run == button presses * 5
       	   binary = secsToCount;
       	   secMin = 0;                                    //set secMin to 0
       	   TACTL |= TASSEL_1 + MC_1 + ID_0 + TACLR;
       	   CCR0 = 32768 - 1;                              //start timerA for 1 second interrupt
           }  
       	
       if (P1IFG & BIT6){	
       	   P1IFG &= ~b_2;
       	   b_2Counter++;
       	   minutes = b_2Counter * 60;
       	   binary = b_2Counter;
       	   secMin = 1;                                   //set secMin to 1
       	   TACTL |= TASSEL_1 + MC_1 + ID_0 + TACLR;
       	   CCR0 = 32768 - 1;                             //start timerA for 1 second interrupt
           }
     
        
   else if (P1IFG & BIT7){	            //reset sequence
       	   P1IFG &= ~bSet;              //clear interrupt 
       	   CCR0 = 0;                    //timer off 
       	   TACTL |= MC_0 + TACLR;       //timera off mode reset counter
       	   TACTL &= ~TAIFG;             //clear timera interrupt
       	   bSetCounter = 0;             //set counters to 0
       	   minutes = 0;
       	   seconds = 0;
       	   b_1Counter = 0;
       	   b_2Counter = 0;
       	   P1OUT &= ~relay;               // turn off relay
       	   
       	   sendInstruction(0x2);         // return home sets DDRAM address to zero
       	   sendDataArray(message2, 12);    // send message using sendDataArray "reset"
       	   delay(500);
       	   mode = clockMode;
       	   return;
           }
           
        CharsToSend();  
        clearDisplay();
        
        if(secMin == 0)
         sendDataArray(message5, 18);   //send set seconds to LCD 	
        else
         sendDataArray(message6, 18);   //send set minutes to LCD
         
         sendDataArray(charsToSend,4);  // value for timer
     }  
    }  
    
   #pragma vector=TIMERA0_VECTOR
   __interrupt void Timer_A (void)
   {
   	
   seconds++;                               //increment seconds
   P1OUT |= relay;                          //turn on relay
   binary = seconds;

   if(seconds == secsToCount){              //seconds
      CCR0 = 0;                             //stop timer
      P1OUT &= ~relay;                      //turn off relay
      seconds = 0;
      b_1Counter = 0;
      b_1Seconds = 0;
      secsToCount = 0;
      timesUp();
      mode = clockMode;
      return;
     }
      
   if(seconds == minutes){                  //minutes
      CCR0 = 0;                             //stop timer
      P1OUT &= ~relay;                      //turn off relay
      seconds = 0;
      minutes = 0;
      b_2Counter = 0;
      timesUp();
      mode = clockMode;
      return;
     }
     
     CharsToSend(); 
     clearDisplay();
     
     unsigned char i;
     for(i = 0 ; i < 19 ; i++)         // send 20 blankspaces to the LCD
     sendData(0x20);
     sendDataArray(charsToSend,4);    // send the timer info to the LCD
  }
