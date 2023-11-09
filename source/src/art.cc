#include <art.h>
#include <common/universalfunc.h>
using namespace os;
using namespace os::common;
void printf(char*);
	void Funny::printfLine(char *str, uint8_t line) {

        	uint16_t attrib = 0x07;

        	volatile uint16_t* vidmem;
		

        	for (int i = 0; str[i] != '\0'; i++) {

        		vidmem = (volatile uint16_t*)0xb8000 + (80*line+i);
	                *vidmem = str[i] | (attrib << 8);

        	}
	}
	//#TODO: END CUBE
	//Used for cube in ascii
	/*float calculateX(int32_t i, int32_t j, int32_t k) {
			  return j * sin(A) * sin(B) * cos(C) - k * cos(A) * sin(B) * cos(C) +
			         j * cos(A) * sin(C) + k * sin(A) * sin(C) + i * cos(B) * cos(C);
			}

			float calculateY(int32_t i, int32_t j, int32_t k) {
			  return j * cos(A) * cos(C) + k * sin(A) * cos(C) -
			         j * sin(A) * sin(B) * sin(C) + k * cos(A) * sin(B) * sin(C) -
			         i * cos(B) * sin(C);
			}

			float calculateZ(int32_t i, int32_t j, int32_t k) {
			  return k * cos(A) * cos(B) - j * sin(A) * cos(B) + i * sin(B);
			}

			void calculateForSurface(float cubeX, float cubeY, float cubeZ, int32_t ch) {
			  x = calculateX(cubeX, cubeY, cubeZ);
			  y = calculateY(cubeX, cubeY, cubeZ);
			  z = calculateZ(cubeX, cubeY, cubeZ) + distanceFromCam;

			  ooz = 1 / z;

			  xp = (int32_t)(width / 2 + horizontalOffset + K1 * ooz * x * 2);
			  yp = (int32_t)(height / 2 + K1 * ooz * y);

			  idx = xp + yp * width;
			  if (idx >= 0 && idx < width * height) {
			    if (ooz > zBuffer[idx]) {
			      zBuffer[idx] = ooz;
			      buffer[idx] = ch;
			    }
			  }
			}

	//Imagine you cant fucking use vectors to handle standard things in math like sinus, cosinus etc. Donut in C tutorials LMAO NIGGERS FROM CIA CANT CODE EASY THINGS
	void Funny::cubeAscii(uint8_t cubeCount) {
			//Generally speaking, its rotating over 
			float A,B,C;
			float cubeWidth = 20;
			int32_t width = 160, height=44;
			float zBuffer[160 * 44];
			int8_t buffer[160 * 44];
			int32_t backgroundASCIICode = '.';
			int32_t distanceFromCam = 100;
			float horizontalOffset;
			float K1 = 40;

			float incrementSpeed = 0.6;
			printf("\x1b[2J");
  			while (1) {
    			memset(buffer, backgroundASCIICode, width * height);
    			memset(zBuffer, 0, width * height * 4);
    			cubeWidth = 20;
    			horizontalOffset = -2 * cubeWidth;
    			// first cube
    			for (float cubeX = -cubeWidth; cubeX < cubeWidth; cubeX += incrementSpeed) {
    			  for (float cubeY = -cubeWidth; cubeY < cubeWidth;
    			       cubeY += incrementSpeed) {
    			    calculateForSurface(cubeX, cubeY, -cubeWidth, '@');
    			    calculateForSurface(cubeWidth, cubeY, cubeX, '$');
    			    calculateForSurface(-cubeWidth, cubeY, -cubeX, '~');
    			    calculateForSurface(-cubeX, cubeY, cubeWidth, '#');
    			    calculateForSurface(cubeX, -cubeWidth, -cubeY, ';');
    			    calculateForSurface(cubeX, cubeWidth, cubeY, '+');
    			  }
    			}
    			cubeWidth = 10;
    			horizontalOffset = 1 * cubeWidth;
    			// second cube
    			for (float cubeX = -cubeWidth; cubeX < cubeWidth; cubeX += incrementSpeed) {
    			  for (float cubeY = -cubeWidth; cubeY < cubeWidth;
    			       cubeY += incrementSpeed) {
    			    calculateForSurface(cubeX, cubeY, -cubeWidth, '@');
    			    calculateForSurface(cubeWidth, cubeY, cubeX, '$');
    			    calculateForSurface(-cubeWidth, cubeY, -cubeX, '~');
    			    calculateForSurface(-cubeX, cubeY, cubeWidth, '#');
    			    calculateForSurface(cubeX, -cubeWidth, -cubeY, ';');
    			    calculateForSurface(cubeX, cubeWidth, cubeY, '+');
    			  }
    			}
    			cubeWidth = 5;
    			horizontalOffset = 8 * cubeWidth;
    			// third cube
    			for (float cubeX = -cubeWidth; cubeX < cubeWidth; cubeX += incrementSpeed) {
    			  for (float cubeY = -cubeWidth; cubeY < cubeWidth;
    			       cubeY += incrementSpeed) {
    			    calculateForSurface(cubeX, cubeY, -cubeWidth, '@');
    			    calculateForSurface(cubeWidth, cubeY, cubeX, '$');
    			    calculateForSurface(-cubeWidth, cubeY, -cubeX, '~');
    			    calculateForSurface(-cubeX, cubeY, cubeWidth, '#');
    			    calculateForSurface(cubeX, -cubeWidth, -cubeY, ';');
    			    calculateForSurface(cubeX, cubeWidth, cubeY, '+');
    			  }
    			}
    			printf("\x1b[H");
    			for (int k = 0; k < width * height; k++) {
    			  printf(k % width ? buffer[k] : 10);
    			}

    			A += 0.05;
    			B += 0.05;
    			C += 0.01;
    			usleep(8000 * 2);
  			}
  		return 0;
	}*/
	void Funny::amsFloppy(){
		printfLine(" ___,___,_______,____  ", 10);
		printfLine("|  :::|///./||'||    \ ", 11);
		printfLine("|  :::|//.//|| || H)  |", 12);
		printfLine("|  :::|/.///|!!!|     |", 13);
		printfLine("|   _______________   |", 14);
		printfLine("|  |:::::::::::::::|  |", 15);
		printfLine("|  |_______________|  |", 16);
		printfLine("|  |_______________|  |", 17);
		printfLine("|  |_______________|  |", 18);
		printfLine("|  |_______________|  |", 19);
		printfLine("||_|   AMS-OS      ||_|", 20);
		printfLine("|__|_______________|__|", 21);
	}


	void Funny::pastMemory() {
		
        	printfLine("       AMS-OS v0.6.5 pre-alpha", 1);
	}

	void Funny::AMS_Art1() {
  	}

	void Funny::AMS_Art2() {
	}


	void Funny::AMS_Art3() {
	}



	void Funny::god() {
	}


