/*
	Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		commands.c
	*
	*	PURPOSE:	This program contains all the commands which can be acted upon by the SSM.
	*
	*				The reason why these commands are not executed within within decode_command()
	*				in can_api.c is purely for organizational convenience.
	*
	*	FILE REFERENCES:		commands.h
	*
	*	EXTERNAL VARIABLES:		send_now, send_data, send_hk, send_coms
	*
	*	EXTERNAL REFERENCES:	Same a File References.
	*
	*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None.
	*
	*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None
	*
	*	NOTES:	None.
	*
	*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
	*	None so far.
	*
	*	DEVELOPMENT HISTORY:
	*	08/2/2015		Created.
	*
	*	08/07/2015		Modified the send... functions in order to comply with the new
	*					CAN structure that I have been working on.
	*	08/08/2015		I modified the send functions again in accordance with the
	*					change that I have made to the CAN structure, this being that
	*					the first byte of a CAN message now contains both the sender's
	*					ID and the destination ID.
	*
	*	08/09/2015		Functions send_read_response(), and send_write_response() have been added.
	*
*/

#include "commands.h"

/************************************************************************/
/* RUN COMMANDS                                                         */
/*																		*/
/* This function loops through a list of global command flags and		*/
/*	executes the corresponding command function if they are equal to 1.	*/
/************************************************************************/

void run_commands(void)
{
	if (send_now == 1)
		send_response();
	if (send_hk == 1)
		send_housekeeping();
	if (send_data == 1)
		send_sensor_data();
	if (send_coms == 1)
		send_coms_packet();
	if (read_response == 1)
		send_read_response();
	if (write_response == 1)
		send_write_response();

	return;	
}

/************************************************************************/
/* SEND RESPONSE                                                        */
/*																		*/
/* Thia function sends a generic response to the generic "REQ_RESPONSE	*/
/* which was issued by the OBC.											*/
/************************************************************************/
void send_response(void)
{
	send_arr[7] = (SELF_ID << 4)|OBC_ID;
	send_arr[6] = MT_COM;
	send_arr[5] = RESPONSE;

	can_send_message(&(send_arr[0]), CAN1_MB7);		//CAN1_MB7 is the command reception MB.
	send_now = 0;
	return;
}

/************************************************************************/
/* SEND HOUSEKEEPING                                                    */
/*																		*/
/* This function is intended to be used to send housekeeping to the OBC.*/
/* It is likely that more than 4 bytes will be required to send the HK	*/
/* from a particular subsystem and hence this function will send a		*/
/* series of CAN messages to the OBC each one with a different smalltype*/
/************************************************************************/

void send_housekeeping(void)
{	
	send_arr[7] = (SELF_ID << 4)|OBC_ID;
	send_arr[6] = MT_HK;	// HK will likely require multiple message in the future.

	can_send_message(&(send_arr[0]), CAN1_MB6);		//CAN1_MB6 is the HK reception MB.
	send_hk = 0;
	return;
}

/************************************************************************/
/* SEND SENSOR DATA                                                     */
/*																		*/
/* This is an example of a generic sensor data transmission function	*/
/* that we will likely have in the future. An SSM would only execute	*/
/* this command upon request from the OBC.								*/
/************************************************************************/

void send_sensor_data(void)
{
	uint8_t high, low;			
	//adc_read(&send_arr[0]);	// This line was used to acquire temp from an analog sensor.
	spi_retrieve_temp(&high, &low);

	send_arr[7] = (SELF_ID << 4)|OBC_ID;
	send_arr[6] = MT_DATA;
	send_arr[5] = SPI_TEMP1;	
	send_arr[1] = high;			// SPI temperature sensor readings.
	send_arr[0] = low;
			
	send_arr[4] = 0x55;			// Temperature indicator.
			
	can_send_message(&(send_arr[0]), CAN1_MB0);		//CAN1_MB0 is the data reception MB.
	send_data = 0;
	
	return;
}

/************************************************************************/
/* SEND COMS PACKET                                                     */
/*																		*/
/* For the time being, this function simply sends a single ASCII		*/
/* character to the OBC when a packet is received from the transceiver. */
/* In the future, this function will be able to send entire packets		*/
/* to the OBC as two CAN messages (2 x 4 bytes).						*/
/************************************************************************/
void send_coms_packet(void)
{			
	send_arr[7] = (SELF_ID << 4)|OBC_ID;
	send_arr[6] = MT_DATA;
	send_arr[5] = COMS_PACKET;
	send_arr[0] = trans_msg[0];	// ASCII character which was received.
	
	can_send_message(&(send_arr[0]), CAN1_MB0);		//CAN1_MB0 is the data reception MB.
	send_coms = 0;
	return;
}
/************************************************************************/
/* SEND READ RESPONSE                                                   */
/*																		*/
/* Whent the OBC requests to read an address in the SSM's memory, this	*/
/* function carries out that task with the use of global array read_arr	*/
/* read_arr stores the most read request from the OBC.					*/
/************************************************************************/

void send_read_response(void)
{
	uint8_t read_val, passkey, req_by;
	uint8_t* read_ptr;
	
	passkey = read_arr[3];
	read_ptr = read_arr[0];
	req_by = read_arr[7];	// Used to coordinating with tasks on the OBC.
	
	/*	Execute the read	*/
	read_val = *read_ptr;
	
	send_arr[7] = (SELF_ID << 4)|req_by;
	send_arr[6] = MT_COM;
	send_arr[5] = ACK_READ;
	send_arr[3] = passkey;
	send_arr[0] = read_val;
	
	can_send_message(&(send_arr[0]), CAN1_MB7);
	read_response = 0;
	return;
}

/************************************************************************/
/* SEND WRITE RESPONSE                                                  */
/*																		*/
/* When the OBC requests to write to an address in the SSM's memory,	*/
/* this function carries out that task with global array write_arr.		*/
/* write_arr stores the most recent write request. -1 is given to the	*/
/* OBC as FAILURE, and 1 is returned as SUCCESS.						*/
/************************************************************************/
void send_write_response(void)
{
	uint8_t passkey, write_data, ret_val, verify, req_by;
	uint8_t* write_ptr;
	
	passkey = write_arr[3];
	write_ptr = write_arr[1];
	write_data = write_arr[0];
	req_by = read_arr[7];	// Used to coordinating with tasks on the OBC.
	
	/*	Execute the Write	*/
	*write_ptr = write_data;
	
	/* Verify the write worked correctly	*/
	verify = *write_ptr;
	if (verify != write_data)
		ret_val = -1;
	else
		ret_val = 1;
	
	send_arr[7] = (SELF_ID << 4)|req_by;
	send_arr[6] = MT_COM;
	send_arr[5] = ACK_WRITE;
	send_arr[3] = passkey;
	send_arr[0] = ret_val;
	
	can_send_message(&(send_arr[0]), CAN1_MB7);
	write_response = 0;
	return;	
}