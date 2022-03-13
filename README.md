# hpex49xled-LINUX

This is a C version of the mediasmartserverd available for HP EX48x. All credit for the code is given to the orginal authors.
This version works on Ubuntu 20.04 and above. It is threaded and contains the code I wrote to revise the mediasmarserverd's activity function. 
The program runs one thread per HP bay. I have incorporated code for each of the supported systems in the original mediasmartserverd program. 
I do not have an H340-H342 or Acer Altos etc. I cannot validate the disk_init code works on these platforms. I am assuming it works on the HP EX48x.

This work was done in preparation for me porting this to FreeBSD. 

Feel free to contact me regarding these platforms and I will work with you on getting the code for the disk_init function for your respective platform.

