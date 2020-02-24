#include <stdio.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "thermalcam.pb.h"

int main()
{
    /* This is the buffer where we will store our message. */
    uint8_t buffer[128];
    size_t message_length;
    bool status;
    
    /* Encode our message */
    {
        /* Allocate space on the stack to store the message data.
         *
         * Nanopb generates simple struct definitions for all the messages.
         * - check out the contents of simple.pb.h!
         * It is a good idea to always initialize your structures
         * so that you do not have garbage data from RAM in there.
         */
        sony_MQTTConfig message = sony_MQTTConfig_init_zero;
        
        /* Create a stream that will write to our buffer. */
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        
        /* Fill in the lucky number */
        strcpy(message.ip, "192.168.1.1");
        message.port = 1883;
        strcpy(message.uuid, "1857d200-ee8c-4ddb-bd3c-77d3c3d6add3");
        
        /* Now we are ready to encode the message! */
        status = pb_encode(&stream, sony_MQTTConfig_fields, &message);
        message_length = stream.bytes_written;
        
        /* Then just check for any errors.. */
        if (!status)
        {
            printf("Encoding failed: %s\n", PB_GET_ERROR(&stream));
            return 1;
        }
        else {
          printf("data: ");
          for (int i = 0; i<stream.bytes_written; i++){
            printf("%02X", buffer[i]);
          }
          printf("\n");
        }
    }
    
    /* Now we could transmit the message over network, store it in a file or
     * wrap it to a pigeon's leg.
     */

    /* But because we are lazy, we will just decode it immediately. */
    
    {
        /* Allocate space for the decoded message. */
        sony_MQTTConfig message = sony_MQTTConfig_init_zero;
        
        /* Create a stream that reads from the buffer. */
        pb_istream_t stream = pb_istream_from_buffer(buffer, message_length);
        
        /* Now we are ready to decode the message. */
        status = pb_decode(&stream, sony_MQTTConfig_fields, &message);
        
        /* Check for errors... */
        if (!status)
        {
            printf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
            return 1;
        }
        
        /* Print the data contained in the message. */
        printf("Your lucky number was %d!\n", (int)message.port);
        printf("ip: %s\n", message.ip);
        printf("uuid: %s\n", message.uuid);
    }
    
    return 0;
}

