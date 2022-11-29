/*
 * mainGrue20.c
 *
 * Created: 2022-04-04
 * Remis: 	2022-04-28
 * Autor :  Samuel Velasco, Dany Morisette, Liam Pander
 * Objectif: Recevoir l'information de la manette et ensuite faire bouger les moteurs correspondants � la vitesse demand�e
 * Contient �galement les limit switch et bloque les moteurs si l'un est activ�
 *
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <util/delay.h>
#include <stdlib.h>
#include "uart.h"
#include "utils.h"
#include "driver.h"
#include "lcd.h"
#include "fifo.h"

#define HORAIRE 1
#define ANTIHORAIRE 0

#define INTERIEUR 0
#define EXTERIEUR 1
#define IMMOBILE 2

#define OFF 0
#define ON 1

#define OFFICIEL 1
#define TEST_LIMIT_SWITCH 0

volatile int16_t cnt = 0;
volatile uint8_t i1 = 0;
volatile uint8_t i0 = 0;

// IMPORTANT: Suite � un probl�me de fonctionnement de notre encodeur la soir�e pr�c�dant la comp�tition,
// 			  nous n'avons jamais utilis� cette partie l� du code pour l'encodeur et nous nous sommes bas�s sur le temps
//			  pour faire la partie automatique

// fct d'interruption sur INT0
ISR(INT0_vect) // External interrupt_zero ISR
{
	if (read_bit(PIND, PD2)) // si le signal vient de monter
		i0 = 0;				 // annuler le flag de la derni�re descente
	else
	{ // si le signal vient de descendre
		if (i1)
		{			// si l'autre signal �tait descendu avant
			cnt--;	// d�crementer le d�compte de clics
			i0 = 0; // remettre les flags � 0
			i1 = 0;
		}
		else		// si l'autre signal n'�tait pas descendu avant
			i0 = 1; // activer le flag de descente de ce signal
	}
}

// fct d'interruption sur INT1 (M�me principe que INT0)
ISR(INT1_vect) // External interrupt_zero ISR
{
	if (read_bit(PIND, PD3)) // filtrer les pulses pollueurs
		i1 = 0;
	else
	{
		if (i0)
		{ // si i0 �tait descendu avant...
			cnt++;
			i0 = 0;
			i1 = 0;
		}
		else
			i1 = 1; // sinon activer le flag de i1
	}
}

#if (OFFICIEL)
int main(void)
{

	// Moteur 1 // J6 // DIR PB0 // PWM PD6 // 298:1 // FLECHE // FIL ORANGE
	// Moteur 2 // J5 // DIR PB2 // PWM PB4 // 210:1 // CHARIOT // FIL JAUNE
	// Moteur 3 // J4 // DIR PB1 // PWM PB3 // 100:1 // HOOK // FIL VERT

	// LES MOTEURS COMMENCENT � TOURNER AVEC UNE VALEUR PLUS GRANDE QUE 25 � PEU PR�S
	char commande[40];
	char message_moteur[40];
	char message_commande[40];
	char encodeur[5];
	char test[10];

	char tab_mli[4];
	int mli = 0;

	char direction_1[2];
	int dir_1 = 2;

	char direction_2[2];
	int dir_2 = 2;

	char direction_3[2];
	int dir_3 = 2;

	char etat_moteur_1[2];
	int etat_1 = 0;

	char etat_moteur_2[2];
	int etat_2 = 0;

	char etat_moteur_3[2];
	int etat_3 = 0;

	int etat_switch0 = 0;
	int etat_switch1 = 0;

	uint8_t hterm;

	// External Interrupt Mask Register: activer int. sur INT0/INT1
	EIMSK = set_bit(EIMSK, INT0);
	EIMSK = set_bit(EIMSK, INT1);

	// external interrupt control register: any edge
	EICRA = set_bit(EICRA, ISC00);
	EICRA = clear_bit(EICRA, ISC01);
	EICRA = set_bit(EICRA, ISC10);
	EICRA = clear_bit(EICRA, ISC11);

	DDRD = clear_bit(DDRD, PD2);
	DDRD = clear_bit(DDRD, PD3);

	PORTD = set_bit(PORTD, PD2);
	PORTD = set_bit(PORTD, PD3);

	// Initialisation des limit switch

	DDRA = clear_bit(DDRA, PA0); // met en entr�e
	DDRA = clear_bit(DDRA, PA1); // met en entr�e

	PORTA = set_bit(PORTA, PA0); // Pull-up  interne
	PORTA = set_bit(PORTA, PA1); // Pull-up interne

	lcd_init();	 // Faire l'initialisation du LCD
	pwm0_init(); // PB3 et PB4
	pwm2_init(); // PD6

	uart_init(UART_0); // Initialisation de la communication
	sei();

	// MOTEURS EN SORTIE
	DDRB = set_bit(DDRB, PB1); // MOTEUR 1
	DDRB = set_bit(DDRB, PB2); // MOTEUR 2
	DDRB = set_bit(DDRB, PB0); // MOTEUR 3

	// VITESSES DES MOTEURS
	// pwm0_set_PB3(50); // MOTEUR 1 // FLECHE
	// pwm0_set_PB4(50); // MOTEUR 2 // CHARIOT
	// pwm2_set_PD6(50); // MOTEUR 3 // HOOK

	// PORTB = set_bit(PORTB, PB1); // MOTEUR 1 // ANTIHORAIRE
	// PORTB = set_bit(PORTB, PB2); // MOTEUR 2 // ANTIHORAIRE
	// PORTB = set_bit(PORTB, PB0); // MOTEUR 3 // ANTIHORAIRE

	// PORTB = clear_bit(PORTB, PB1); // MOTEUR 1 // HORAIRE
	// PORTB = clear_bit(PORTB, PB1); // MOTEUR 2 // HORAIRE
	// PORTB = clear_bit(PORTB, PB1); // MOTEUR 3 // HORAIRE

	// pwm1_set_PD5(2000); // ANGLE DU SERVOMOTEUR // 1000 = 0 // 1500 = 45 // 2000 = 90

	while (1)
	{

		if (uart_rx_buffer_nb_line(UART_0) > 0) // R�ceptivit� du string avec les donn�es en ordres : MLI(3) , ETAT 1 , ETAT 2 , ETAT 3 , DIR 1 , DIR 2 , DIR 3
		{
			uart_get_line(UART_0, commande, 40);
		}

		// envoie de l'�tat de l'encodeur
		sprintf(encodeur, "%3d\n", cnt);
		uart_put_string(UART_0, encodeur);

		// Check de l'�tat des limit switch
		etat_switch0 = read_bit(PINA, PA0);
		etat_switch1 = read_bit(PINA, PA1);

		// Cr�ation du MLI � partir du string ( position 0 ,1 ,2 )
		tab_mli[0] = commande[0];
		tab_mli[1] = commande[1];
		tab_mli[2] = commande[2];
		tab_mli[3] = '\0';
		mli = atoi(tab_mli);

		// Cr�ation de l'�tat des moteurs a partir du string ( position 3 , 4 , 5 )
		etat_moteur_1[0] = commande[3];
		etat_moteur_1[1] = '\0';
		etat_moteur_2[0] = commande[4];
		etat_moteur_2[1] = '\0';
		etat_moteur_3[0] = commande[5];
		etat_moteur_3[1] = '\0';
		etat_1 = atoi(etat_moteur_1);
		etat_2 = atoi(etat_moteur_2);
		etat_3 = atoi(etat_moteur_3);

		// Cr�ation des directions a partir du string (position 6 ,7 , 8 )
		direction_1[0] = commande[6];
		direction_1[1] = '\0';
		direction_2[0] = commande[7];
		direction_2[1] = '\0';
		direction_3[0] = commande[8];
		direction_3[1] = '\0';
		dir_1 = atoi(direction_1);
		dir_2 = atoi(direction_2);
		dir_3 = atoi(direction_3);

		// D�termination du moteur 1 ( FLECHE ) ***********************************************************************************************

		if (etat_1 == 0)
		{
			pwm0_set_PB3(0); // MOTEUR 1 ARRET�
		}

		else if (etat_1 == 1)
		{
			if (dir_1 == 2) // ZONE MORTE, le moteur ne doit pas bouger, on ne lui donne pas de direction
			{
				pwm0_set_PB3(0); // MOTEUR 1 ARRET�
			}
			else if (dir_1 == 0) // moteur est HORAIRE
			{
				pwm0_set_PB3(mli);			   // MOTEUR 1
				PORTB = clear_bit(PORTB, PB1); // MOTEUR 1 // HORAIRE
			}
			else if (dir_1 == 1) // moteur est ANTIHORAIRE
			{
				pwm0_set_PB3(mli);			 // MOTEUR 1
				PORTB = set_bit(PORTB, PB1); // MOTEUR 1 // ANTIHORAIRE
			}
		}

		// D�termination du moteur 2 ( CHARIOT ) ***************************************************************************************************

		if (etat_switch0 == FALSE && etat_switch1 == FALSE) // Si aucune des limit switchs est activ�
		{
			if (etat_2 == 0)
			{
				pwm0_set_PB4(0); // MOTEUR 2 ARRET�
			}
			else if (etat_2 == 1)
			{
				if (dir_2 == 2) // ZONE MORTE, le moteur ne doit pas bouger, on ne lui donne pas de direction
				{
					pwm0_set_PB4(0); // MOTEUR 2 ARRET�
				}
				else if (dir_2 == 0) // moteur est HORAIRE
				{
					pwm0_set_PB4(mli);			   // MOTEUR 2
					PORTB = clear_bit(PORTB, PB2); // MOTEUR 2 // HORAIRE
				}
				else if (dir_2 == 1) // moteur est ANTIHORAIRE
				{
					pwm0_set_PB4(mli);			 // MOTEUR 2
					PORTB = set_bit(PORTB, PB2); // MOTEUR 2 // ANTIHORAIRE
				}
			}
		}

		if (etat_switch0 == TRUE) // Limite switch au bout de la fl�che est activ�
		{
			if (etat_2 == 0)
			{
				pwm0_set_PB4(0); // MOTEUR 2 ARRET�
			}
			else if (etat_2 == 1)
			{
				if (dir_2 == IMMOBILE) // ZONE MORTE, le moteur ne doit pas bouger, on ne lui donne pas de direction
				{
					pwm0_set_PB4(0); // MOTEUR 2 ARRET�
				}
				else if (dir_2 == INTERIEUR) // moteur est HORAIRE (moteur recule)
				{
					pwm0_set_PB4(mli);			   // MOTEUR 2
					PORTB = clear_bit(PORTB, PB2); // MOTEUR 2 // HORAIRE
				}
				else if (dir_2 == EXTERIEUR) // moteur est ANTIHORAIRE
				{
					pwm0_set_PB4(0); // MOTEUR 2 ARRET�
				}
			}
		}
		if (etat_switch1 == TRUE) // Limite switch au centre(monkey Treee) de la fl�che est activ�
		{
			if (etat_2 == OFF)
			{
				pwm0_set_PB4(0); // MOTEUR 2 ARRET�
			}
			else if (etat_2 == ON)
			{
				if (dir_2 == IMMOBILE) // ZONE MORTE, le moteur ne doit pas bouger, on ne lui donne pas de direction
				{
					pwm0_set_PB4(0); // MOTEUR 2 ARRET�
				}
				else if (dir_2 == INTERIEUR) // moteur est HORAIRE
				{
					pwm0_set_PB4(0); // MOTEUR 2 ARRET�
				}
				else if (dir_2 == EXTERIEUR) // moteur est ANTIHORAIRE ( moteur avance)
				{
					pwm0_set_PB4(mli);			 // MOTEUR 2
					PORTB = set_bit(PORTB, PB2); // MOTEUR 2 // ANTIHORAIRE
				}
			}
		}

		// D�termination du moteur 3 ( HOOK )
		if (etat_3 == 0)
		{
			pwm2_set_PD6(0); // MOTEUR 3 ARRET�
		}
		else if (etat_3 == 1)
		{
			if (dir_3 == 2) // ZONE MORTE, le moteur ne doit pas bouger, on ne lui donne pas de direction
			{
				pwm2_set_PD6(0); // MOTEUR 3 ARRET�
			}
			else if (dir_3 == 0) // moteur est HORAIRE
			{
				pwm2_set_PD6(mli);			   // MOTEUR 3
				PORTB = clear_bit(PORTB, PB0); // MOTEUR 3 // HORAIRE
			}
			else if (dir_3 == 1) // moteur est ANTIHORAIRE
			{
				pwm2_set_PD6(mli);			 // MOTEUR 3
				PORTB = set_bit(PORTB, PB0); // MOTEUR 3 // ANTIHORAIRE
			}
		}

		lcd_set_cursor_position(0, 0); // set la premiere ligne avant de print la premiere ligne

		// Settage du message afficher � l'�cran ( MOTEUR 1)

		if (etat_1 == 1)
		{
			lcd_write_string("MOTEUR = 1 :)   ");
		}
		else if (etat_2 == 1)
		{
			lcd_write_string("MOTEUR = 2 :)   ");
		}
		else if (etat_3 == 1)
		{
			lcd_write_string("MOTEUR = 3 :)   ");
		}
		else
		{
			lcd_write_string("TOM GET LA BEERZ");
		}

		// Code pour le message de la ligne du bas
		// moteur 1

		if (dir_1 == 1 && etat_1 == 1)
		{
			sprintf(message_commande, "MLI%3d DIR DROIT", mli);
			lcd_set_cursor_position(0, 1);
			lcd_write_string(message_commande);
		}
		else if (dir_1 == 0 && etat_1 == 1)
		{
			sprintf(message_commande, "MLI%3d DIR GAUCH", mli);
			lcd_set_cursor_position(0, 1);
			lcd_write_string(message_commande);
		}
		else if (dir_1 == 2)
		{
			sprintf(message_commande, "MLI%3d DIR TOMMM", mli);
			lcd_set_cursor_position(0, 1);
			lcd_write_string(message_commande);
		}

		// moteur 2 message

		if (dir_2 == 1 && etat_2 == 1)
		{
			sprintf(message_commande, "MLI%3d DIR DROIT", mli);
			lcd_set_cursor_position(0, 1);
			lcd_write_string(message_commande);
		}
		else if (dir_2 == 0 && etat_2 == 1)
		{
			sprintf(message_commande, "MLI%3d DIR GAUCH", mli);
			lcd_set_cursor_position(0, 1);
			lcd_write_string(message_commande);
		}
		else if (dir_2 == 2)
		{
			sprintf(message_commande, "MLI%3d DIR TOMMM", mli);
			lcd_set_cursor_position(0, 1);
			lcd_write_string(message_commande);
		}

		// moteur 3 message

		if (dir_3 == 1 && etat_3 == 1)
		{
			sprintf(message_commande, "MLI%3d DIR DROIT", mli);
			lcd_set_cursor_position(0, 1);
			lcd_write_string(message_commande);
		}
		else if (dir_3 == 0 && etat_3 == 1)
		{
			sprintf(message_commande, "MLI%3d DIR GAUCH", mli);
			lcd_set_cursor_position(0, 1);
			lcd_write_string(message_commande);
		}
		else if (dir_3 == 2)
		{

			sprintf(message_commande, "MLI%3d DIR TOMMM", mli);
			lcd_set_cursor_position(0, 1);
			lcd_write_string(message_commande);
		}
	}
}
#endif

#if (TEST_LIMIT_SWITCH)

// Petit programme qui sert a tester le fonctionnement des limit switches;
int main(void)
{

	char test[10];
	lcd_init();
	lcd_set_cursor_position(0, 0);

	DDRA = clear_bit(DDRA, PA0); // met en entr�e
	DDRA = clear_bit(DDRA, PA1); // met en entr�e
	PORTA = set_bit(PORTA, PA0); // Pull-up  interne
	PORTA = set_bit(PORTA, PA1); // Pull-up interne

	int etat_switch1 = 0;
	int etat_switch2 = 0;

	while (1)
	{
		etat_switch1 = read_bit(PORTA, PA0);
		etat_switch2 = read_bit(PORTA, PA1);

		sprintf(test, "%d", cnt);
		lcd_write_string(test);

		lcd_set_cursor_position(0, 1);
		sprintf(test, "%d_SW1   %d_SW2", etat_switch1, etat_switch2);
		lcd_write_string(test);

		_delay_ms(50);
	}
}
#endif