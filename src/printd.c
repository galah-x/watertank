#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
#include "print.h"

void printd2(uint16_t n)
{
  char msb;
  char lsb;
  lsb = n % 10;
  msb = n / 10;
  if (msb > 9)
    msb = msb + 'A' - '0';
  if (msb != 0) 
    pchar(msb + '0' );
  pchar(lsb + '0' );
}

void printd3(uint16_t n)
{
  char lsb;
  lsb = n % 10;
  n = n / 10;
  printd2(n);
  pchar(lsb + '0' );
}

void printd2_1(int16_t n)
{
  char decimal;

  if (n < 0)
    {
      pchar('-');
      n = 0 - n;   // positive from here on
    }

  decimal = (n % 10)  +  '0' ;
  n = n / 10;

  printd2(n);
  pchar('.');
  pchar(decimal);
  
}

void printd6_3(uint32_t n)
{
  char d0;
  char d1;
  char d2;
  char i0;

//  if (n < 0)
//    {
//      pchar('-');
//      n = 0 - n;   // positive from here on
//    }

  d0 = (n % 10)  +  '0' ;
  n = n / 10;
  d1 = (n % 10)  +  '0' ;
  n = n / 10;
  d2 = (n % 10)  +  '0' ;
  n = n / 10;
  i0 = (n % 10)  +  '0' ;
  n = n / 10;

  printd2((uint16_t)n);
  pchar(i0);
  pchar('.');
  pchar(d2);
  pchar(d1);
  pchar(d0);
}

void printd6_2(uint32_t n)
{
  char d0;
  char d1;
  char i0;
  char i1;
  
//  if (n < 0)
//    {
//      pchar('-');
//      n = 0 - n;   // positive from here on
//    }

  d0 = (n % 10)  +  '0' ;
  n = n / 10;
  d1 = (n % 10)  +  '0' ;
  n = n / 10;
  i0 = (n % 10)  +  '0' ;
  n = n / 10;
  i1 = (n % 10)  +  '0' ;
  n = n / 10;

  printd2((uint16_t)n);
  pchar(i1);
  pchar(i0);
  pchar('.');
  pchar(d1);
  pchar(d0);
}

