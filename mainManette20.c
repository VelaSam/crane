/*
 * mainManette20.c
 *
 * Created: 2022-03-28
 * Remis: 	2022-04-28
 * Autor : Samuel Velasco, Liam Pander, Dany Morissette
 */ 

//Fonction qui permet de recevoir une certaine valeur en int et qui renvoie son état selon les 3 define (INTERIEUR, EXTERIEUR ou IMMOBILE);
int valeur_a_etat(int valeur);

//Fonction retournant un string qui reçois un certain état (dépendant des mêmes 3 define plus haut) et renvoie un string à imprimer sur l'écran LCD
const char* etat_a_string(int etat);

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>
#include <stdlib.h>

#include "uart.h"
#include "utils.h"
#include "driver.h"
#include "lcd.h"
#include "fifo.h"

#define TRUE 1
#define FALSE 0

#define MODE_MANUEL 0
#define MODE_AUTOMATIQUE 1

#define MODE_DEPLACEMENT 0
#define MODE_CROCHET 1

#define TROIS_SECONDES 30

#define LED1 PB4
#define LED2 PB3
#define LED3 PB2

#define DROITE_GAUCHE 0
#define HAUT_BAS 1
#define SLIDER 3

#define INTERIEUR 0
#define EXTERIEUR 1
#define IMMOBILE 2


#define EQ20_CODE_PRINCIPAL	1
#define EQ20_SMOKE_ON_THE_WATER 0

#define ZONE_MORTE_MINIMUM 55
#define ZONE_MORTE_MAXIMUM 200

#if(EQ20_CODE_PRINCIPAL)
int main(void)
{	

	//LIMIT SWITCH
	DDRA = clear_bit(DDRA, PA0);
	DDRA = clear_bit(DDRA, PA1);
	PORTA = set_bit(PORTA, PA0); 
	PORTA = set_bit(PORTA, PA1); 
	
	
	//JOYSTICK
	DDRA = clear_bit(DDRA, PA2); 
	PORTA = set_bit(PORTA, PA2);
	
	//DIODES ELECTROLUMINESCENTES
	DDRB = set_bits(DDRB, 0b00011111);
	
	//SWITCH 1, SWITCH 2, SWITCH 3
	DDRD = clear_bit(DDRD, PD5);
	DDRD = clear_bit(DDRD, PD6);
	DDRD = clear_bit(DDRD, PD7);
	PORTD = set_bit(PORTD, PD5);
	PORTD = set_bit(PORTD, PD6);
	PORTD = set_bit(PORTD, PD7);
	
	//Initilalisation du LCD
	lcd_init();
	//Initialisation du adc
	adc_init();
	
	//Initialisation de la partie uart et communication
	uart_init(UART_0);
	sei();

	//Valeur lue horizontale du joystick, valeur verticale du joystick et valeur du potentiomètre 
	uint8_t x_read = 0, y_read = 0, slider_read = 0;
	
	//Mode Principal qui peut à tout moment switcher entre MODE_MANUEL et MODE_AUTOMATIQUE
	int mode_principal = MODE_MANUEL;
	//Un certain compteur de boucles dans le programme, lorsque la variable atteint une certaine valeur, le mode_principal change
	int compteur_boucles = 0;
	
	//String a imprimer sur l'ecran LCD 
	char string_position[40];
	
	//String a envoyer au PCB de la grue
	char string_commandes[40];
	
	//Variables qui gardent l'état de la direction pour les trois moteurs
	int etat_premier;//Moteur de la flèche
	int etat_deuxiem;//Moteur du chariot
	int etat_troisie;//Moteur du crochet
	
	//Variables numériques qui permettent de vérifier l'état des LEDS sur PB4, PB3 et PB2
	int trans_1 = 0;
	int trans_2 = 0;
	int trans_3 = 0;
	
	//Variables booléennes qui permettent de vérfifier l'état physique des boutons sw1, sw2, sw3
	bool sw1_state = FALSE;
	bool sw2_state = FALSE;
	bool sw3_state = FALSE;
	
	//Variables booléennes qui permettent de garder l'état physique des boutons sw1, sw2, sw3 à la boucle précédente
	bool sw1_precedent = FALSE;
	bool sw2_precedent = FALSE;
	bool sw3_precedent = FALSE;
	
	//Un string qui affiche sur l'écran LCD soit (IMMOBILE, EXTERIEUR ou INTERIEUR)
	const char* str_mouvement_joystick = NULL;
	
	// Variable qui contiendra l'état du bouton du joystick
	bool button_state;
	
	//Forcer la première étape pour l'étape automatique
	int etape_automatique = 1;
	
	//message de commencement de la partie automatique
	int announcing = 0;
	
	while(1)
	{
		/*   DEBUT DE LA PARTIE SUR LES 3 SWITCH */
		//Ce bout de code permet de lire si les boutons sont appuyés et ensuite allumer la DEL correspondante
		//et ensuite stocker cet état dans une variable qui sera ensuite envoyée sur UART à l'autre PCB
		//Il est a noter que si un des trois état est activé, l'autre état se fait désactiver automatiquement
		sw1_precedent = sw1_state;
		sw2_precedent = sw2_state;
		sw3_precedent = sw3_state;
		
		
		sw1_state = read_bit(PIND, PD7);
		sw2_state = read_bit(PIND, PD6);
		sw3_state = read_bit(PIND, PD5);
		
		if(sw1_precedent != sw1_state){
			
			if(!sw1_state && !trans_1){
				PORTB = set_bit(PORTB, LED1);
				PORTB = clear_bit(PORTB, LED2);
				PORTB = clear_bit(PORTB, LED3);
				
			}
			else if(!sw1_state && trans_1){
				PORTB = clear_bit(PORTB, LED1);
			}
		}
		
		if(sw2_precedent != sw2_state){
			
			if(!sw2_state && !trans_2){
				PORTB = set_bit(PORTB, LED2);
				PORTB = clear_bit(PORTB, LED3);
				PORTB = clear_bit(PORTB, LED1);
				
			}
			else if(!sw2_state && trans_2){
				PORTB = clear_bit(PORTB, LED2);
			}
		}
		
		if(sw3_precedent != sw3_state){
			
			if(!sw3_state && !trans_3){
				PORTB = set_bit(PORTB, LED3);
				PORTB = clear_bit(PORTB, LED2);
				PORTB = clear_bit(PORTB, LED1);
				
			}
			else if(!sw3_state && trans_3){
				PORTB = clear_bit(PORTB, LED3);
			}
		}
		/*   FIN DE LA PARTIE SUR LES 3 SWITCH */
		
		//Compter le temps (a peu pres 3 secondes) qu'on garde le bouton du joystick appuyé
		//Lire etat du joystick
		button_state = read_bit(PINA, PA2);
		
		if(compteur_boucles >= TROIS_SECONDES){
			if(mode_principal == MODE_MANUEL)
			{
				mode_principal = MODE_AUTOMATIQUE;
			}
			else if(mode_principal == MODE_AUTOMATIQUE)
			{
				mode_principal= MODE_MANUEL;
			}
			
			compteur_boucles = 0;
		}
		if(!button_state)
			compteur_boucles++;
		else
			compteur_boucles = 0;
		
		
		
		lcd_set_cursor_position(0,0);
		
		
		//execute commandes principales dependant si on est dans le mode Manuel ou mode automatique
		switch(mode_principal){
			
			case MODE_MANUEL:
				announcing = 0;
				
				
				x_read = adc_read(DROITE_GAUCHE);
				y_read = adc_read(HAUT_BAS);
				slider_read = abs(adc_read(SLIDER)-255); //Il faut inverser la valeur en faisant la valeur absolue de la valeur -255
				
				etat_premier = valeur_a_etat(x_read);
				etat_deuxiem = valeur_a_etat(y_read);
				etat_troisie = valeur_a_etat(y_read);
				
				trans_1 = read_bit(PORTB, LED1);
				trans_2 = read_bit(PORTB, LED2);
				trans_3 = read_bit(PORTB, LED3);
				
				lcd_set_cursor_position(0,0);
				
				
				
				
				if(trans_1){
					lcd_write_string("Moteur: Fleche  ");
					str_mouvement_joystick = etat_a_string(etat_premier);
				
					slider_read *= 0.65;//BLOQUER LE MLI À 65% DE SA CAPACITÉ POUR LE PREMIER MOTEUR SINON LA FLECHE DEBALANCE
				} 
				else if(trans_2){
					lcd_write_string("Moteur: Chariot ");
					str_mouvement_joystick = etat_a_string(etat_deuxiem);
				}
					
				else if(trans_3){
					lcd_write_string("Moteur: Crochet ");
					str_mouvement_joystick = etat_a_string(etat_troisie);
					
				}
					
				else{
					lcd_write_string("Moteur: AUCUN   ");
				}
				
				lcd_set_cursor_position(0,1);
				sprintf(string_position, "MLI%3d %s", slider_read, str_mouvement_joystick);
				lcd_write_string(string_position);
		
				//PARTIE ENVOYÉE A LAUTRE PCB string_commandes
				sprintf(string_commandes,"%3d%d%d%d%1d%1d%1d", slider_read, trans_1, trans_2, trans_3 , etat_premier, etat_deuxiem, etat_troisie);
				strcat(string_commandes, "\n");
				uart_put_string(UART_0, string_commandes);
				
				_delay_ms(50);
			
			break;

			case MODE_AUTOMATIQUE:
			
				if(announcing == 0){
					lcd_clear_display();
					lcd_write_string("AUTOMATIQUE");
				
					//DÉCOMPTE DE 2 Secondes
					_delay_ms(500);
					lcd_clear_display();
					lcd_write_string("2");
					_delay_ms(1000);
					lcd_clear_display();
					lcd_write_string("1");
					_delay_ms(1000);
					lcd_clear_display();
					announcing = 1;
				}
				
				
				for(etape_automatique = 1; etape_automatique < 4; etape_automatique++)
				{
					lcd_clear_display();
					lcd_set_cursor_position(0,0);
					lcd_write_string("tourne");
					uart_put_string(UART_0,"080100100\n"); // MLI 50  , MOTEUR FLECHE  = ON , DIRECTION = DROITE (interieur)
					_delay_ms(4470);
					uart_put_string(UART_0,"000000000\n");
					_delay_ms(1000);
				
					uart_put_string(UART_0, "150010000\n"); // MLI = 50 , MOTEUR CHARIOT = ON , DIRECTION = RECULE
					_delay_ms(9000);
					lcd_clear_display();
					lcd_set_cursor_position(0,0);
					lcd_write_string("recule");
					uart_put_string(UART_0,"000000000\n");
					_delay_ms(1000);
				
					uart_put_string(UART_0, "080100100\n"); // MLI = 50 , MOTEUR CHARIOT = ON , DIRECTION = DROITE (intérieur)
					_delay_ms(4470);
					lcd_clear_display();
					lcd_set_cursor_position(0,0);
					lcd_write_string("tourne");
					uart_put_string(UART_0,"000000000\n");
					_delay_ms(1000);
				
					uart_put_string(UART_0, "150010010\n"); // MLI = 50 , MOTEUR CHARIOT = ON , DIRECTION = AVANCE
					_delay_ms(9000);
					lcd_clear_display();
					lcd_set_cursor_position(0,0);
					lcd_write_string("avance");
					uart_put_string(UART_0,"000000000\n");
					_delay_ms(1000);
				}
				
				lcd_clear_display();
				lcd_write_string("AUTOMATIQUE FINI");
				_delay_ms(10000);
				
				mode_principal = MODE_MANUEL;
				
		
			break;
		}
	}
}
#endif


int valeur_a_etat(int valeur){
	
	int etat;
	
	if(valeur < ZONE_MORTE_MINIMUM)
		etat = INTERIEUR;
	
	else if(valeur > ZONE_MORTE_MAXIMUM)
		etat = EXTERIEUR;
		
	else
		etat = IMMOBILE;
		
	return etat;
}

const char* etat_a_string(int etat){
	
	const char* str_renvoi = NULL;
	
	if(etat == INTERIEUR) //0
		str_renvoi = "Interieur";
		
	else if(etat == EXTERIEUR) //1
		str_renvoi = "Exterieur";
		
	else if(etat == IMMOBILE) //2
		str_renvoi = "Immobile ";
		
	return str_renvoi;
}

//Petit programme qui fait jouer la chanson "Smoke on the Water" par Deep Purple en faisant tourner les moteurs a une certaine vitesse
//Crédit: Samuel Velasco, son perfect pitch et son temps a perdre
#if(EQ20_SMOKE_ON_THE_WATER)
int main(void){

	lcd_init();
	adc_init();
	uart_init(UART_0);
	sei();
	
	while(1){

		lcd_clear_display();
		lcd_write_string("SMOKE ON THE WATER");

		//255 == C5 6
		//244 == B 5
		//226 == A# 4
		//213 == A 3
		//206 == G# 2
		//194 == G 1
		//182 == F# 0
		
		uart_put_string(UART_0, "182111111\n");
		_delay_ms(500);
		
		uart_put_string(UART_0, "000000000\n");
		_delay_ms(50);

		uart_put_string(UART_0, "213111111\n");
		_delay_ms(500);
		
		uart_put_string(UART_0, "000000000\n");
		_delay_ms(50);
		
		uart_put_string(UART_0, "244111111\n");
		_delay_ms(800);
		
		uart_put_string(UART_0, "000000000\n");
		_delay_ms(50);

		uart_put_string(UART_0, "182111111\n");
		_delay_ms(500);
		
		uart_put_string(UART_0, "000000000\n");
		_delay_ms(50);
		
		uart_put_string(UART_0, "213111111\n");
		_delay_ms(500);
		
		uart_put_string(UART_0, "000000000\n");
		_delay_ms(50);

		uart_put_string(UART_0, "255111111\n");
		_delay_ms(250);
		
		uart_put_string(UART_0, "000000000\n");
		_delay_ms(50);

		uart_put_string(UART_0, "244111111\n");
		_delay_ms(900);
		
		uart_put_string(UART_0, "000000000\n");
		_delay_ms(50);
		
		uart_put_string(UART_0, "182111111\n");
		_delay_ms(500);
		
		uart_put_string(UART_0, "000000000\n");
		_delay_ms(50);
		
		uart_put_string(UART_0, "213111111\n");
		_delay_ms(500);
		
		uart_put_string(UART_0, "000000000\n");
		_delay_ms(50);
		
		uart_put_string(UART_0, "244111111\n");
		_delay_ms(800);
		
		uart_put_string(UART_0, "000000000\n");
		_delay_ms(50);
		
		uart_put_string(UART_0, "213111111\n");
		_delay_ms(500);
		
		uart_put_string(UART_0, "000000000\n");
		_delay_ms(50);
		
		uart_put_string(UART_0, "182111111\n");
		_delay_ms(800);

		uart_put_string(UART_0, "000111111\n");
		_delay_ms(1000);

	}

}
#endif
