/******************************************************************************* 
SerialCommand - An Arduino library to tokenize and parse commands received over
a serial port. 
Based on 2011-2013 Steven Cogswell  <steven.cogswell@gmail.com>
Copyright 2017 Ngo Huynh Ngoc Khanh <ngohuynhngockhanh@gmail.com>
http://arduino.vn

Version 2017.   

Version History:
May 11 2011 - Initial version
May 13 2011 -	Prevent overwriting bounds of SerialCommandCallback[] array in addCommand()
			defaultHandler() for non-matching commands
Mar 2012 - Some const char * changes to make compiler happier about deprecated warnings.  
           Arduino 1.0 compatibility (Arduino.h header) 
Oct 2013 - SerialCommand object can be created using a SoftwareSerial object, for SoftwareSerial
           support.  Requires #include <SoftwareSerial.h> in your sketch even if you don't use 
           a SoftwareSerial port in the project.  sigh.   See Example Sketch for usage. 
Oct 2013 - Conditional compilation for the SoftwareSerial support, in case you really, really
           hate it and want it removed.  
Feb 2017 - Allow command and arguments into two buffers JSON

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************************/

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "SerialCommand.h"


#include <string.h>
#ifndef SERIALCOMMAND_HARDWAREONLY
#include <SoftwareSerial.h>
#endif

// Constructor makes sure some things are set. 
SerialCommand::SerialCommand()
{
	usingSoftwareSerial=0;
	nowReading = false; //means read eventName
	strncpy(delim," ",MAXDELIMETER);  // strtok_r needs a null-terminated string
	term='\r';   // return character, default terminator for commands
	numCommand=0;    // Number of callback handlers installed
	clearBuffer(); 
	addedDefaultCommand = false;
}

#ifndef SERIALCOMMAND_HARDWAREONLY
// Constructor to use a SoftwareSerial object
SerialCommand::SerialCommand(SoftwareSerial &_SoftSer)
{
	usingSoftwareSerial=1; 
	SoftSerial = &_SoftSer;
	nowReading = false; //means read eventName
	strncpy(delim," ",MAXDELIMETER);  // strtok_r needs a null-terminated string
	term='\r';   // return character, default terminator for commands
	numCommand=0;    // Number of callback handlers installed
	clearBuffer(); 
	addedDefaultCommand = false;
}
#endif


//
// Initialize the command buffer being processed to all null characters
//
void SerialCommand::clearBuffer(char type)
{
	if (type == 0 || type == 2) {
		for (int i=0; i<SERIALCOMMANDBUFFER; i++) 
		{
			buffer[i]='\0';
		}
		bufPos=0; 
	}
	if (type == 1 || type == 2) {
		for (int i=0; i<ARGUMENTBUFFER; i++) 
		{
			bufferArg[i]='\0';
		}
		bufArgPos = 0;
	}
	
	
}

// Retrieve the next token ("word" or "argument") from the Command buffer.  
// returns a NULL if no more tokens exist.   
char *SerialCommand::next() 
{
	char *nextToken = bufferArg;
	return nextToken; 
}

// This checks the Serial stream for characters, and assembles them into a buffer.  
// When the terminator character (default '\r') is seen, it starts parsing the 
// buffer for a prefix command, and calls handlers setup by addCommand() member
void SerialCommand::readSerial() 
{
	char prev = 'a';
	// If we're using the Hardware port, check it.   Otherwise check the user-created SoftwareSerial Port
	#ifdef SERIALCOMMAND_HARDWAREONLY
	while (Serial.available() > 0) 
	#else
	while ((usingSoftwareSerial==0 && Serial.available() > 0) || (usingSoftwareSerial==1 && SoftSerial->available() > 0) )
	#endif
	{
		int i; 
		boolean matched; 
		if (usingSoftwareSerial==0) {
			// Hardware serial port
			inChar=Serial.read();   // Read single available character, there may be more waiting
		} else {
			#ifndef SERIALCOMMAND_HARDWAREONLY
			// SoftwareSerial port
			inChar = SoftSerial->read();   // Read single available character, there may be more waiting
			#endif
		}
		#ifdef SERIALCOMMANDDEBUG
		Serial.print(inChar);   // Echo back to serial stream
		#endif
		if (inChar==term && prev != term) {     // Check for the terminator (default '\r') meaning end of command
			#ifdef SERIALCOMMANDDEBUG
			Serial.print("Received: "); 
			Serial.println(buffer);
		    #endif
			bufPos=0;           // Reset to start of buffer
			#ifdef SERIALCOMMANDDEBUG
			Serial.print("nowReading: ");
			Serial.println(nowReading);
			#endif
			if (!nowReading) { //read buffer
				token = strtok_r(buffer,delim,&last);   // Search for command at start of buffer
				command = token;
				if (command.indexOf("}") > -1) {
					nowReading = !nowReading;//kiểm tra command có } thì nó bị lỗi => hoán vị lại!
				}
				if (token == NULL) return; 
			} else {
				matched=false; 
				for (i=0; i<numCommand; i++) {
					#ifdef SERIALCOMMANDDEBUG
					Serial.print("Comparing ["); 
					Serial.print(token); 
					Serial.print("] to [");
					Serial.print(CommandList[i].command);
					Serial.println("]");
					#endif
					// Compare the found command against the list of known commands for a match
					if (strncmp(token,CommandList[i].command,SERIALCOMMANDBUFFER) == 0) 
					{
						#ifdef SERIALCOMMANDDEBUG
						Serial.print("Matched Command: "); 
						Serial.println(token);
						#endif
						// Execute the stored handler function for the command
						(*CommandList[i].function)(); 
						clearBuffer(); 
						matched=true; 
						break; 
					}
				}
				if (matched==false) {
					if (addedDefaultCommand)
						(*defaultHandler)(command); 
					clearBuffer(); 
				}
			}
			nowReading = !nowReading;//hoasn vij laij ahihi
		}
		if (isprint(inChar))   // Only printable characters into the buffer
		{
			if (!nowReading) {
				buffer[bufPos++]=inChar;   // Put character into buffer
				buffer[bufPos]='\0';  // Null terminate
				if (bufPos > SERIALCOMMANDBUFFER-1) bufPos=0; // wrap buffer around if full  
			} else {
				bufferArg[bufArgPos++]=inChar;   // Put character into bufferArg
				bufferArg[bufArgPos]='\0';  // Null terminate
				if (bufArgPos > ARGUMENTBUFFER-1) bufArgPos=0; // wrap buffer around if full  
			}		
			prev = inChar;
		}
	}
}

// Adds a "command" and a handler function to the list of available commands.  
// This is used for matching a found token in the buffer, and gives the pointer
// to the handler function to deal with it. 
void SerialCommand::addCommand(const char *command, void (*function)())
{
	if (numCommand < MAXSERIALCOMMANDS) {
		#ifdef SERIALCOMMANDDEBUG
		Serial.print(numCommand); 
		Serial.print("-"); 
		Serial.print("Adding command for "); 
		Serial.println(command); 
		#endif
		
		strncpy(CommandList[numCommand].command,command,SERIALCOMMANDBUFFER); 
		CommandList[numCommand].function = function; 
		numCommand++; 
	} else {
		// In this case, you tried to push more commands into the buffer than it is compiled to hold.  
		// Not much we can do since there is no real visible error assertion, we just ignore adding
		// the command
		#ifdef SERIALCOMMANDDEBUG
		Serial.println("Too many handlers - recompile changing MAXSERIALCOMMANDS"); 
		#endif 
	}
}

// This sets up a handler to be called in the event that the receveived command string
// isn't in the list of things with handlers.
void SerialCommand::addDefaultHandler(void (*function)(String))
{
	defaultHandler = function;
	addedDefaultCommand = true;
}