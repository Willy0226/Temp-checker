#include "mbed.h"
#include "XNucleoIKS01A2.h"

Serial pc(USBTX, USBRX); //serial connection to terminal
Mutex serial_mutex; //this will ensure that no two threads access the serial connection at the same time.
Mutex sensor_mutex; //this will ensure that no two threads access the sensor board at the same time.
DigitalOut led(PA_5);

Thread thread;  //output thread
Thread thread2; //flag thread

float tempRead, tempC, tempF, tempK, tempR, humidity; //will hold temperature and humidity readings
bool flag = false; //flag to indicate that temperature and humidity should be read and output

//sensors
static XNucleoIKS01A2 *sensor_board = XNucleoIKS01A2::instance(D14, D15, D4, D5);
static HTS221Sensor *hum_temp = sensor_board->ht_sensor; //temperature and humidity sensor
static LSM303AGRAccSensor *acc = sensor_board->accelerometer; //accelerometer

//output temperature/humidity
void output_thread() {
	while(1) {
    	sensor_mutex.lock();
    	hum_temp->get_temperature(&tempRead); //gets the temperature in celcius
    	hum_temp->get_humidity(&humidity); //gets the humidity in %
    	sensor_mutex.unlock();
    	if (flag) {
        	tempC = tempRead; //tempC will get temperature in celsius
        	tempF = (tempC * 1.8f) + 32; //to convert temperature in celsius to fahrenheit
        	tempK = tempC + 273.15f; //to convert temperature in celsius to kelvin
        	tempR = tempF + 459.67f; //to convert temperature in fahrenheit to rankine

        	serial_mutex.lock();
        	pc.printf("Temperature: %.2f C, %.2f F, %.2f K, %.2f R \r\n", tempC, tempF, tempK, tempR); // Output temperature to console
        	pc.printf("Humidity: %.2f%% \r\n", humidity);
        	if(tempF > 80 && humidity > 50) {
            	led = 1;
            	pc.printf("Drink water, use sun screen, stay out of the sun! \r\n");
        	}
        	if(tempF < 70 && humidity < 30) {
            	led = 1;
            	pc.printf("Keep warm and wear a jacket! \r\n");
        	}
        	serial_mutex.unlock();
        	ThisThread::sleep_for(1000); //wait 1 second
        	led = 0;
        	flag = false;
    	}
	}
}

void flag_thread() {
	int ystate = 0, xstate = 0; //state of shaking progress
	Timer tm; //timer to track time since the last motion
	tm.start();
	while(1) {
    	int32_t axes[3];
    	if (!flag) {
        	sensor_mutex.lock();
        	acc->get_x_axes(axes);
        	sensor_mutex.unlock();
    	}
    	if (abs(axes[1]) > 800 && ystate == 0) { //begin y axis shake
        	if (axes[1] > 800){
            	ystate++;
            	tm.reset();
        	} else {
            	ystate--;
            	tm.reset();
        	}
    	} else if (abs(axes[1]) > 800 && abs(ystate) == 1 && tm.read_ms() >= 130) {
        	if (axes[1] > 800 && ystate == -1){
            	ystate--;
            	tm.reset();
        	} else if (axes[1] < -800 && ystate == 1) {
            	ystate++;
            	tm.reset();
        	}
    	} else if (abs(axes[1]) > 800 && abs(ystate) == 2 && tm.read_ms() >= 130) {
        	if (axes[1] < -800 && ystate == -2){
            	ystate--;
            	tm.reset();
        	} else if (axes[1] > 800 && ystate == 2) {
            	ystate++;
            	tm.reset();
        	}
    	} else if (abs(axes[0]) > 800 && xstate == 0 && tm.read_ms() >= 130){ //begin x axis shake, y axis is prioritized over x as it is much more likely and this reduces possible confusion
        	if (axes[0] > 800){
            	xstate++;
            	tm.reset();
        	} else {
            	xstate--;
            	tm.reset();
        	}
    	} else if (abs(axes[0]) > 800 && abs(xstate) == 1 && tm.read_ms() >= 130) {
        	if (axes[0] > 800 && xstate == -1){
            	xstate--;
            	tm.reset();
        	} else if (axes[0] < -800 && xstate == 1) {
            	xstate++;
            	tm.reset();
        	}
    	} else if (abs(axes[0]) > 800 && abs(xstate) == 2 && tm.read_ms() >= 130) {
        	if (axes[0] < -800 && xstate == -2){
            	xstate--;
            	tm.reset();
        	} else if (axes[0] > 800 && xstate == 2) {
            	xstate++;
            	tm.reset();
        	}
    	}
    	if (abs(ystate) == 3 || abs(xstate) == 3) { //Shaking detected, set the flag
        	ystate = xstate = 0; //reset shake
        	tm.reset();
        	flag = true;
    	} else if (tm.read_ms() >= 1000) { //reset after 1 second without further movement
        	ystate = xstate = 0;
        	tm.reset();
    	}
	}
}

//clears terminal screen
void clear() {
	pc.printf("%c",char(27)); //ESC
	pc.printf("[2J");     	//Clear screen
	pc.printf("%c",char(27)); //ESC
	pc.printf("[H");      	//Cursor to home
}

int main() {

	acc->enable(); //enable the accelerometer
	hum_temp->enable(); //enable the temperature and humidity sensor
    
	clear(); //terminal screen is cleared when board is reset
    
	//start both threads
	thread.start(output_thread);
	thread2.start(flag_thread);
}

