#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <wiringPi.h>
#include <sched.h>
#include <errno.h>
#include <string.h>

#define MODE_Pause 0
#define MODE_Start1 1
#define MODE_Recor1 2
#define MODE_Start2 3
#define MODE_Recor2 4

#define BUFFER_Coeff 2
#define BUFFER1_Size 5000
#define BUFFER2_Size BUFFER1_Size*BUFFER_Coeff

#define couleur(param) printf("\033[%sm",param)
/*   param devant être un const char *, vide (identique à "0") ou formé
     d'une où plusieurs valeurs séparées par des ; parmi
         0  réinitialisation         1  haute intensité (des caractères)
         5  clignotement             7  video inversé
         30, 31, 32, 33, 34, 35, 36, 37 couleur des caractères
         40, 41, 42, 43, 44, 45, 46, 47 couleur du fond
            les couleurs, suivant la logique RGB, étant respectivement
               noir, rouge, vert, jaune, bleu, magenta, cyan et blanc */

#define max(a,b) (a>=b?a:b)
#define min(a,b) (a<=b?a:b)

typedef struct _Signal
{
	struct _Signal *Suivant;
	int	Occ;
	int	Deb;
} SIGNAL;

typedef struct _TblAsso
{
	struct _TblAsso *Suivant;
	int	Cle;
	int	Val;
} TBLASSO;

typedef struct _Serie4
{
	struct _Serie4 *Suivant;
	int	Occ;
	unsigned int	Val1;
	unsigned int	Val2;
	unsigned int	Val3;
	unsigned int	Val4;
} SERIE4;

int g_PinReception;
unsigned short int g_InterruptBuf1[BUFFER1_Size];
unsigned short int g_InterruptBuf2[BUFFER2_Size];
unsigned char g_InterruptBub2[BUFFER2_Size];
unsigned char g_InterruptBut2[BUFFER2_Size];
struct timeval g_InterruptTime;
volatile int g_bModePause;
volatile int g_InterruptPos;

SIGNAL	*LstSignal=NULL;
SERIE4	*LstSerie4=NULL;
SERIE4	g_Serie1Min, g_Serie1Max, g_Serie2Min, g_Serie2Max;
int		LngEmission;
int		NbEmissionOk;


/****************************************************************************************************************/
/* Temps réel																									*/
int scheduler_realtime()
{
	struct sched_param p;

	p.__sched_priority = sched_get_priority_max(SCHED_RR);
	if(sched_setscheduler(0, SCHED_RR, &p ) == -1) 
	{
		printf("Impossible d'activer le mode temps réel.\n\r");
		return -1;
	}
	return 0;
}

int scheduler_standard()
{
	struct sched_param p;

	p.__sched_priority = 0;
	if(sched_setscheduler(0, SCHED_OTHER, &p) == -1)
	{
		printf("Impossible de désactiver le mode temps réel.\n\r");
		return -1;
	}
	return 0;
}

void Radio_Interrupt()
{
	struct timeval t;
	unsigned long micros;
	unsigned long valeur;
	int max;
	static int etat;


	switch(g_bModePause)
	{
		case MODE_Pause:
			return;

		case MODE_Start1:
		case MODE_Start2:
			gettimeofday(&g_InterruptTime, NULL);
			g_InterruptPos=0;
			g_bModePause++;
			etat = LOW;
			return;
	}

	//*** Changement d'état
/*	if(etat==digitalRead(g_PinReception)) return;
	if(etat==LOW)
		etat = HIGH;
	else
		etat = LOW;
*/
	//*** Longueur du front précédent en µs
	gettimeofday(&t, NULL);
    if (t.tv_sec > g_InterruptTime.tv_sec) micros = 1000000L; else micros = 0;
    micros += (t.tv_usec - g_InterruptTime.tv_usec);

	//*** Mémoriser le début du front suivant
	g_InterruptTime.tv_sec = t.tv_sec;
	g_InterruptTime.tv_usec = t.tv_usec;

	//*** Mémoriser le chrono dans un des buffers
	if(g_bModePause==MODE_Recor1)
	{
		g_InterruptBuf1[g_InterruptPos++] = micros;
		if(g_InterruptPos==BUFFER1_Size)
		{
			g_bModePause = MODE_Pause;
			return;
		}
	}

	if(g_bModePause==MODE_Recor2)
	{
		g_InterruptBuf2[g_InterruptPos] = micros;

		g_InterruptBub2[g_InterruptPos] = digitalRead(g_PinReception);
		gettimeofday(&t, NULL);
		micros = (t.tv_usec - g_InterruptTime.tv_usec);
		g_InterruptBut2[g_InterruptPos] = micros;

		g_InterruptPos++;
		if(g_InterruptPos==BUFFER2_Size)
		{
			g_bModePause = MODE_Pause;
			return;
		}
	}

	return;
}

/****************************************************************************************************************/
/* Déclenche la mémorisation																					*/
float Memorisation(int mode)
{
	struct timeval t0;
	struct timeval t1;
	int Pourcentage;
	int PourcentageMemo;
	int TailleBuffer;
	char StrMode[16];


	if(mode==MODE_Start1) 
	{
		TailleBuffer = BUFFER1_Size;
		strcpy(StrMode, "du bruit");
	}
	if(mode==MODE_Start2)
	{
		TailleBuffer = BUFFER2_Size;
		strcpy(StrMode, "de l'emission");
	}

	PourcentageMemo = -1;
	gettimeofday(&t0, NULL);

	g_bModePause = mode;
	while(g_bModePause!=MODE_Pause)
	{
		Pourcentage=g_InterruptPos*100/TailleBuffer;
		if(PourcentageMemo!=Pourcentage)
		{
			PourcentageMemo=Pourcentage;
			printf("\rMemorisation %s : %d %%", StrMode, Pourcentage);
			fflush(stdout);
		}
		delay(10);
	}
	gettimeofday(&t1, NULL);
	printf("\rMemorisation %s : 100 %%\n", StrMode);

    return t1.tv_sec - t0.tv_sec;
}

void AfficheAide()
{
	couleur("37;1");
	printf("RadioAnalyse\n\r");
	couleur("0;37");
	printf("	Analyse des ondes radio 433Mhz pour tenter d'identifier\n\r");
	printf("	un protocole pour le service xAP-Radio.\n\r");
	couleur("37;1");
	printf("Synopsis\n\r");
	couleur("0;37");
	printf("	RadioAnalyse -Pn -I -R -O\n\r");
	couleur("37;1");
	printf("Paramètres\n\r");
	couleur("0;37");
	printf("	-Pn : Pin sur lequel est connecté le récepteur radio.\n\r");
	printf("	-I : Réutilise le dernier jeu de données enregistré sur le pin n.\n\r");
	printf("	-R : Bascule en mode temps réel.\n\r");
	printf("	-O : Génère le fichier data.txt contenant les données analysées.\n\r");
	printf("\n\r");
}

void Analyse1(float DureeBruit, float DureeEmission)
{
	float Rapport;


	couleur("37;1");
	printf("--Rapport bruit/émission------------------------------------------------------\n\r");
	couleur("0;37");
	printf("Durée du bruit %.0f s\n\r", DureeBruit);
	printf("Durée de l'émission %.0f s\n\r", DureeEmission);
	Rapport = DureeBruit/DureeEmission;
	printf("Rapport bruit/émission %.2f\n\r", Rapport);
	printf("\t\033[31mTrop faible\033[37m < 2 < \033[33mUn peu faible\033[37m < 4 < \033[32mCorrect\n\r");
	if(Rapport<2) { couleur("31"); printf("Le rapport est trop faible (Récepteur HS, ou trés mauvaise réception).\n\r"); }
	if((Rapport>=2)&&(Rapport<4)) { couleur("33"); printf("Le rapport est un peu faible (Réception à améliorer).\n\r"); }
	if(Rapport>=4) { couleur("32"); printf("Le rapport est correct.\n\r"); }
	printf("\n\r");
}

int Analyse2()
{
	int i,j,k;
	int OccMemo, Occ, Echec;
	int DureeMin, DureeMax;
	int IndMemo;
	float Rapport;


	couleur("37;1");
	printf("--Recherche d'une constante de temps------------------------------------------\n\r");
	couleur("0;37");
	OccMemo = 0;
	for(i=0; i<BUFFER2_Size/4; i++)
	{
		k = g_InterruptBuf2[i]+g_InterruptBuf2[i+1]+g_InterruptBuf2[i+2]+g_InterruptBuf2[i+3];
		DureeMin = k-(k*10/100);
		DureeMax = k+(k*10/100);
		Occ = 0;
		Echec = 0;
		for(j=i; j<(BUFFER2_Size-4);)
		{
			k = g_InterruptBuf2[j]+g_InterruptBuf2[j+1]+g_InterruptBuf2[j+2]+g_InterruptBuf2[j+3];
			if((k>=DureeMin)&&(k<=DureeMax))
			{
				Occ++;
				Echec = 0;
			}
			else
				Echec++;

			if(Echec>100) j=BUFFER2_Size;	
			if(k>(DureeMax+DureeMax/2))
				j+=1;
			else
				j+=4;
		}
		if(Occ>OccMemo)
		{
			OccMemo = Occ;
			IndMemo = i;
		}
	}
	k = g_InterruptBuf2[IndMemo]+g_InterruptBuf2[IndMemo+1]+g_InterruptBuf2[IndMemo+2]+g_InterruptBuf2[IndMemo+3];
	printf("Constante de temps : %d ms\n\r", k);
	printf("Série de 4 mesures de même longueur %d\n\r", OccMemo);
	Rapport = (float)BUFFER2_Size/(float)(OccMemo*4);
	printf("Rapport mesures totales/mesures identifiées %.2f\n\r", Rapport);
	printf("\t\033[31mTrop important\033[37m > 3 > \033[33mUn peu important\033[37m > 2 > \033[32mCorrect\n\r");
	if(Rapport>3) { couleur("31"); printf("Le rapport est trop important (Récepteur HS, ou trés mauvaise réception).\n\r"); }
	if((Rapport<=3)&&(Rapport>2)) { couleur("33"); printf("Le rapport est un peu trop important (Réception à améliorer).\n\r"); }
	if(Rapport<=2) { couleur("32"); printf("Le rapport est correct.\n\r"); }
	printf("\n\r");

	return IndMemo;
}

void Analyse3(int IndMemo)
{
	int i,j,k;
	int DureeMin, DureeMax;
	int Occ, Echec;
	int PosMemo;
	SIGNAL	*Signal;
	TBLASSO	*TblAsso;
	TBLASSO	*AssoMax;
	TBLASSO	*LstAsso=NULL;
	float Rapport;


	couleur("37;1");
	printf("--Recherche du nombre et de la longueur des émissions-------------------------\n\r");
	couleur("0;37");
	k = g_InterruptBuf2[IndMemo]+g_InterruptBuf2[IndMemo+1]+g_InterruptBuf2[IndMemo+2]+g_InterruptBuf2[IndMemo+3];
	DureeMin = k-(k*10/100);
	DureeMax = k+(k*10/100);
	Occ = 0;
	PosMemo = -1;
	Echec = 0;
	for(j=IndMemo; j<(BUFFER2_Size-4);)
	{
		k = g_InterruptBuf2[j]+g_InterruptBuf2[j+1]+g_InterruptBuf2[j+2]+g_InterruptBuf2[j+3];
		if((k>=DureeMin)&&(k<=DureeMax))
		{
			if(PosMemo == -1) PosMemo = j;
			Occ++;
		}
		else
		{
			Echec++;
			if(PosMemo != -1)
			{
				if(Occ>10) 
				{
					Signal = malloc(sizeof(SIGNAL));
					if(Signal==NULL) 
					{
						printf("Pas assez de mémoire (1).");
						exit;
					}
					Signal->Occ = Occ;
					Signal->Deb = PosMemo;
					Signal->Suivant = LstSignal;
					LstSignal = Signal;
				}
				Occ = 0;
				PosMemo = -1;
			}
		}

		if(k>(DureeMax+DureeMax/2))
			j+=1;
		else
			j+=4;
	}

	//*** Cumuler les occurrences identiques
	i=0;
	for(Signal=LstSignal; Signal!=NULL; Signal=Signal->Suivant)
	{
		i++;
		k=0;
		for(TblAsso=LstAsso; ((TblAsso!=NULL)&&(k==0)); TblAsso=TblAsso->Suivant)
		{
			if(TblAsso->Cle==Signal->Occ)
			{
				k=1;
				TblAsso->Val++;
			}

		}
		if(k==0)
		{
			TblAsso = malloc(sizeof(TBLASSO));
			if(TblAsso==NULL) 
			{
				printf("Pas assez de mémoire (2).");
				exit;
			}
			TblAsso->Cle = Signal->Occ;
			TblAsso->Val = 1;
			TblAsso->Suivant = LstAsso;
			LstAsso = TblAsso;
		}
	}

	printf("Emissions totales : %d\n\r", i);
	AssoMax = LstAsso;
	for(TblAsso=LstAsso; TblAsso!=NULL; TblAsso=TblAsso->Suivant)
	{
		if(TblAsso->Val > AssoMax->Val) AssoMax = TblAsso;
	}
	printf("%d émissions comprenant %d série de 4\n\r", AssoMax->Val, AssoMax->Cle);
	LngEmission = AssoMax->Cle;
	NbEmissionOk= AssoMax->Val;
	Rapport = (float)i/(float)AssoMax->Val;
	printf("Rapport émissions totales/émissions identifiées %.2f\n\r", Rapport);
	printf("\t\033[31mTrop important\033[37m > 3 > \033[33mUn peu important\033[37m > 2 > \033[32mCorrect\n\r");
	if(Rapport>3) { couleur("31"); printf("Le rapport est trop important (Récepteur HS, ou trés mauvaise réception).\n\r"); }
	if((Rapport<=3)&&(Rapport>2)) { couleur("33"); printf("Le rapport est un peu trop important (Réception à améliorer).\n\r"); }
	if(Rapport<=2) { couleur("32"); printf("Le rapport est correct.\n\r"); }

	TblAsso=LstAsso;
	while(TblAsso!=NULL)
	{
		AssoMax = TblAsso;
		TblAsso = TblAsso->Suivant;
		free(AssoMax);
	}

	printf("\n\r");
}

void Serie_Raz(SERIE4 *Serie)
{
	Serie->Val1 = 0;
	Serie->Val2 = 0;
	Serie->Val3 = 0;
	Serie->Val4 = 0;
}

void Serie_Equ(SERIE4 *SerieDst, SERIE4 SerieSrc)
{
	SerieDst->Val1 = SerieSrc.Val1;
	SerieDst->Val2 = SerieSrc.Val2;
	SerieDst->Val3 = SerieSrc.Val3;
	SerieDst->Val4 = SerieSrc.Val4;
}

void Serie_Add(SERIE4 *SerieDst, SERIE4 SerieSrc)
{
	SerieDst->Val1 += SerieSrc.Val1;
	SerieDst->Val2 += SerieSrc.Val2;
	SerieDst->Val3 += SerieSrc.Val3;
	SerieDst->Val4 += SerieSrc.Val4;
}

void Serie_Sub(SERIE4 *SerieDst, SERIE4 SerieSrc)
{
	SerieDst->Val1 -= SerieSrc.Val1;
	SerieDst->Val2 -= SerieSrc.Val2;
	SerieDst->Val3 -= SerieSrc.Val3;
	SerieDst->Val4 -= SerieSrc.Val4;
}

void Serie_Mul(SERIE4 *Serie, float Coef)
{
	Serie->Val1 = (float)Serie->Val1*Coef;
	Serie->Val2 = (float)Serie->Val2*Coef;
	Serie->Val3 = (float)Serie->Val3*Coef;
	Serie->Val4 = (float)Serie->Val4*Coef;
}

void Serie_Div(SERIE4 *Serie, float Coef)
{
	Serie->Val1 = (float)Serie->Val1/Coef;
	Serie->Val2 = (float)Serie->Val2/Coef;
	Serie->Val3 = (float)Serie->Val3/Coef;
	Serie->Val4 = (float)Serie->Val4/Coef;
}

void Analyse4()
{
	int i;
	int ok;
	int max1;
	int max2;
	SIGNAL	*Signal;
	SERIE4	*Serie4;
	SERIE4	SerieA;
	float Rapport;

	couleur("37;1");
	printf("--Recherche les différentes séries de 4 mesures-------------------------------\n\r");
	couleur("0;37");
	for(Signal=LstSignal; Signal!=NULL; Signal=Signal->Suivant)
	{
		for(i=0; i<Signal->Occ; i++)
		{
			SerieA.Val1 = g_InterruptBuf2[Signal->Deb+i*4];
			SerieA.Val2 = g_InterruptBuf2[Signal->Deb+i*4+1];
			SerieA.Val3 = g_InterruptBuf2[Signal->Deb+i*4+2];
			SerieA.Val4 = g_InterruptBuf2[Signal->Deb+i*4+3];

			ok = 0;
			for(Serie4=LstSerie4; (Serie4!=NULL)&&(ok==0); Serie4=Serie4->Suivant)
			{
				if( ((float)Serie4->Val1/1.4 < SerieA.Val1) && ((float)Serie4->Val1*1.4 > SerieA.Val1)
				&&  ((float)Serie4->Val2/1.4 < SerieA.Val2) && ((float)Serie4->Val2*1.4 > SerieA.Val2)
				&&  ((float)Serie4->Val3/1.4 < SerieA.Val3) && ((float)Serie4->Val3*1.4 > SerieA.Val3)
				&&  ((float)Serie4->Val4/1.4 < SerieA.Val4) && ((float)Serie4->Val4*1.4 > SerieA.Val4) )
				{
					ok = 1;
					Serie4->Occ++;
				}
			}
			if(ok==0)
			{
				Serie4 = malloc(sizeof(SERIE4));
				if(Serie4==NULL)
				{
					printf("Pas assez de mémoire (3).");
					exit;
				}
				Serie4->Occ = 1;
				Serie4->Val1 = SerieA.Val1;
				Serie4->Val2 = SerieA.Val2;
				Serie4->Val3 = SerieA.Val3;
				Serie4->Val4 = SerieA.Val4;
				Serie4->Suivant = LstSerie4;
				LstSerie4 = Serie4;
				//printf("Nouvelle série deb %d : %d-%d-%d-%d\n\r", Signal->Deb+i*4, Serie4->Val1, Serie4->Val2, Serie4->Val3, Serie4->Val4);
			}
		}
	}

	for(Serie4=LstSerie4; Serie4!=NULL; Serie4=Serie4->Suivant)
	{
		if(Serie4->Occ>max1)
		{
			max2 = max1;
			g_Serie2Max.Val1 = g_Serie1Max.Val1;
			g_Serie2Max.Val2 = g_Serie1Max.Val2;
			g_Serie2Max.Val3 = g_Serie1Max.Val3;
			g_Serie2Max.Val4 = g_Serie1Max.Val4;
			max1 = Serie4->Occ;
			g_Serie1Max.Val1 = Serie4->Val1;
			g_Serie1Max.Val2 = Serie4->Val2;
			g_Serie1Max.Val3 = Serie4->Val3;
			g_Serie1Max.Val4 = Serie4->Val4;
		}
		else if(Serie4->Occ>max2)
		{
			max2 = Serie4->Occ;
			g_Serie2Max.Val1 = Serie4->Val1;
			g_Serie2Max.Val2 = Serie4->Val2;
			g_Serie2Max.Val3 = Serie4->Val3;
			g_Serie2Max.Val4 = Serie4->Val4;
		}
		//printf("%d x %d-%d-%d-%d\n\r", Serie4->Occ, Serie4->Val1, Serie4->Val2, Serie4->Val3, Serie4->Val4);
	}
	printf("%d séries %d-%d-%d-%d\n\r", max1, g_Serie1Max.Val1, g_Serie1Max.Val2, g_Serie1Max.Val3, g_Serie1Max.Val4);
	printf("%d séries %d-%d-%d-%d\n\r", max2, g_Serie2Max.Val1, g_Serie2Max.Val2, g_Serie2Max.Val3, g_Serie2Max.Val4);

	Rapport = (float)BUFFER2_Size/(float)((max1+max2)*4);
	printf("Rapport séries totales/séries identifiées %.2f\n\r", Rapport);
	printf("\t\033[31mTrop important\033[37m > 3 > \033[33mUn peu important\033[37m > 2 > \033[32mCorrect\n\r");
	if(Rapport>3) { couleur("31"); printf("Le rapport est trop important (Récepteur HS, ou trés mauvaise réception).\n\r"); }
	if((Rapport<=3)&&(Rapport>2)) { couleur("33"); printf("Le rapport est un peu trop important (Réception à améliorer).\n\r"); }
	if(Rapport<=2) { couleur("32"); printf("Le rapport est correct.\n\r"); }

	printf("\n\r");
}

void Analyse5(double coef)
{
	int i;
	SIGNAL	*Signal;
	SERIE4	SerieA, Serie1Moy, Serie2Moy;
	SERIE4	Serie1RlMin, Serie1RlMax, Serie2RlMin, Serie2RlMax;
	SERIE4	Serie1ThMin, Serie1ThMax, Serie2ThMin, Serie2ThMax;
	float Rapport;
	FILE *Hdl;


	Serie_Equ(&Serie1RlMin, g_Serie1Max);		
	Serie_Equ(&Serie1RlMax, g_Serie1Max);		
	Serie_Equ(&Serie2RlMin, g_Serie2Max);
	Serie_Equ(&Serie2RlMax, g_Serie2Max);

	Serie_Equ(&Serie1ThMin, g_Serie1Max);		
	Serie_Equ(&Serie1ThMax, g_Serie1Max);		
	Serie_Equ(&Serie2ThMin, g_Serie2Max);
	Serie_Equ(&Serie2ThMax, g_Serie2Max);

	Serie_Mul(&Serie1ThMax, coef);
	Serie_Mul(&Serie2ThMax, coef);
	Serie_Div(&Serie1ThMin, coef);
	Serie_Div(&Serie2ThMin, coef);

	if((Serie1ThMax.Val1<Serie2RlMin.Val1)||(Serie1ThMin.Val1>Serie2RlMin.Val1))
	{
		i = abs(Serie2RlMin.Val1-Serie1RlMin.Val1)/2;
		if(Serie1RlMin.Val1>Serie2RlMin.Val1)
		{
			Serie1ThMin.Val1 = Serie1RlMin.Val1-i;
			Serie2ThMax.Val1 = Serie2RlMin.Val1+i;
		}
		else
		{
			Serie1ThMax.Val1 = Serie1RlMin.Val1-i;
			Serie2ThMin.Val1 = Serie2RlMin.Val1+i;
		}
	}

	if((Serie1ThMax.Val2<Serie2RlMin.Val2)||(Serie1ThMin.Val2>Serie2RlMin.Val2))
	{
		i = abs(Serie2RlMin.Val2-Serie1RlMin.Val2)/2;
		if(Serie1RlMin.Val2>Serie2RlMin.Val2)
		{
			Serie1ThMin.Val2 = Serie1RlMin.Val2-i;
			Serie2ThMax.Val2 = Serie2RlMin.Val2+i;
		}
		else
		{
			Serie1ThMax.Val2 = Serie1RlMin.Val2-i;
			Serie2ThMin.Val2 = Serie2RlMin.Val2+i;
		}
	}

	if((Serie1ThMax.Val3<Serie2RlMin.Val3)||(Serie1ThMin.Val3>Serie2RlMin.Val3))
	{
		i = abs(Serie2RlMin.Val3-Serie1RlMin.Val3)/2;
		if(Serie1RlMin.Val3>Serie2RlMin.Val3)
		{
			Serie1ThMin.Val3 = Serie1RlMin.Val3-i;
			Serie2ThMax.Val3 = Serie2RlMin.Val3+i;
		}
		else
		{
			Serie1ThMax.Val3 = Serie1RlMin.Val3-i;
			Serie2ThMin.Val3 = Serie2RlMin.Val3+i;
		}
	}

	if((Serie1ThMax.Val4<Serie2RlMin.Val4)||(Serie1ThMin.Val4>Serie2RlMin.Val4))
	{
		i = abs(Serie2RlMin.Val4-Serie1RlMin.Val4)/2;
		if(Serie1RlMin.Val4>Serie2RlMin.Val4)
		{
			Serie1ThMin.Val4 = Serie1RlMin.Val4-i;
			Serie2ThMax.Val4 = Serie2RlMin.Val4+i;
		}
		else
		{
			Serie1ThMax.Val4 = Serie1RlMin.Val4-i;
			Serie2ThMin.Val4 = Serie2RlMin.Val4+i;
		}
	}

	if((Serie2ThMax.Val1<Serie1RlMin.Val1)||(Serie2ThMin.Val1>Serie1RlMin.Val1))
	{
		i = abs(Serie1RlMin.Val1-Serie2RlMin.Val1)/2;
		if(Serie2RlMax.Val1>Serie1RlMin.Val1)
		{
			Serie2ThMin.Val1 = Serie2RlMin.Val1-i;
			Serie1ThMax.Val1 = Serie1RlMin.Val1+i;
		}
		else
		{
			Serie2ThMax.Val1 = Serie2RlMin.Val1+i;
			Serie1ThMin.Val1 = Serie1RlMin.Val1-i;
		}
	}

	if((Serie2ThMax.Val2<Serie1RlMin.Val2)||(Serie2ThMin.Val2>Serie1RlMin.Val2))
	{
		i = abs(Serie1RlMin.Val2-Serie2RlMin.Val2)/2;
		if(Serie2RlMin.Val2>Serie1RlMin.Val2)
		{
			Serie2ThMin.Val2 = Serie2RlMin.Val2-i;
			Serie1ThMax.Val2 = Serie1RlMin.Val2+i;
		}
		else
		{
			Serie2ThMax.Val2 = Serie2RlMin.Val2+i;
			Serie1ThMin.Val2 = Serie1RlMin.Val2-i;
		}
	}

	if((Serie2ThMax.Val3<Serie1RlMin.Val3)||(Serie2ThMin.Val3>Serie1RlMin.Val3))
	{
		i = abs(Serie1RlMin.Val3-Serie2RlMin.Val3)/2;
		if(Serie2RlMin.Val3>Serie1RlMin.Val3)
		{
			Serie2ThMin.Val3 = Serie2RlMin.Val3-i;
			Serie1ThMax.Val3 = Serie1RlMin.Val3+i;
		}
		else
		{
			Serie2ThMax.Val3 = Serie2RlMin.Val3+i;
			Serie1ThMin.Val3 = Serie1RlMin.Val3-i;
		}
	}

	if((Serie2ThMax.Val4<Serie1RlMin.Val4)||(Serie2ThMin.Val4>Serie1RlMin.Val4))
	{
		i = abs(Serie1RlMin.Val4-Serie2RlMin.Val4)/2;
		if(Serie2RlMin.Val4>Serie1RlMin.Val4)
		{
			Serie2ThMin.Val4 = Serie2RlMin.Val4-i;
			Serie1ThMax.Val4 = Serie1RlMin.Val4+i;
		}
		else
		{
			Serie2ThMax.Val4 = Serie2RlMin.Val4+i;
			Serie1ThMin.Val4 = Serie1RlMin.Val4-i;
		}
	}

	couleur("37;1");
	printf("--Recherche des mini/maxi sur les 2 séries de 4 mesures-----------------------\n\r");
	couleur("0;37");

	Serie_Raz(&Serie1Moy);
	Serie_Raz(&Serie2Moy);

	Serie1RlMax.Occ = 0;
	Serie2RlMax.Occ = 0;

	for(Signal=LstSignal; Signal!=NULL; Signal=Signal->Suivant)
	{
		for(i=0; i<Signal->Occ; i++)
		{
			SerieA.Val1 = g_InterruptBuf2[Signal->Deb+i*4];
			SerieA.Val2 = g_InterruptBuf2[Signal->Deb+i*4+1];
			SerieA.Val3 = g_InterruptBuf2[Signal->Deb+i*4+2];
			SerieA.Val4 = g_InterruptBuf2[Signal->Deb+i*4+3];

			if(		(Serie1ThMax.Val1>SerieA.Val1)&&(Serie1ThMin.Val1<SerieA.Val1)
				&&	(Serie1ThMax.Val2>SerieA.Val2)&&(Serie1ThMin.Val2<SerieA.Val2)
				&&	(Serie1ThMax.Val3>SerieA.Val3)&&(Serie1ThMin.Val3<SerieA.Val3)
				&&	(Serie1ThMax.Val4>SerieA.Val4)&&(Serie1ThMin.Val4<SerieA.Val4))
			{
				Serie1RlMax.Occ++;
				Serie1RlMax.Val1 = max(Serie1RlMax.Val1, SerieA.Val1);
				Serie1RlMax.Val2 = max(Serie1RlMax.Val2, SerieA.Val2);
				Serie1RlMax.Val3 = max(Serie1RlMax.Val3, SerieA.Val3);
				Serie1RlMax.Val4 = max(Serie1RlMax.Val4, SerieA.Val4);
				Serie1RlMin.Val1 = min(Serie1RlMin.Val1, SerieA.Val1);
				Serie1RlMin.Val2 = min(Serie1RlMin.Val2, SerieA.Val2);
				Serie1RlMin.Val3 = min(Serie1RlMin.Val3, SerieA.Val3);
				Serie1RlMin.Val4 = min(Serie1RlMin.Val4, SerieA.Val4);
				Serie_Add(&Serie1Moy, SerieA);
			}
			if(		(Serie2ThMax.Val1>SerieA.Val1)&&(Serie2ThMin.Val1<SerieA.Val1)
				&&	(Serie2ThMax.Val2>SerieA.Val2)&&(Serie2ThMin.Val2<SerieA.Val2)
				&&	(Serie2ThMax.Val3>SerieA.Val3)&&(Serie2ThMin.Val3<SerieA.Val3)
				&&	(Serie2ThMax.Val4>SerieA.Val4)&&(Serie2ThMin.Val4<SerieA.Val4))
			{
				Serie2RlMax.Occ++;
				Serie2RlMax.Val1 = max(Serie2RlMax.Val1, SerieA.Val1);
				Serie2RlMax.Val2 = max(Serie2RlMax.Val2, SerieA.Val2);
				Serie2RlMax.Val3 = max(Serie2RlMax.Val3, SerieA.Val3);
				Serie2RlMax.Val4 = max(Serie2RlMax.Val4, SerieA.Val4);
				Serie2RlMin.Val1 = min(Serie2RlMin.Val1, SerieA.Val1);
				Serie2RlMin.Val2 = min(Serie2RlMin.Val2, SerieA.Val2);
				Serie2RlMin.Val3 = min(Serie2RlMin.Val3, SerieA.Val3);
				Serie2RlMin.Val4 = min(Serie2RlMin.Val4, SerieA.Val4);
				Serie_Add(&Serie2Moy, SerieA);
			}
		}
	}
	Serie_Div(&Serie1Moy, Serie1RlMax.Occ);
	Serie_Div(&Serie2Moy, Serie2RlMax.Occ);

	printf("Bornes Mini/Maxi\n\r");
	printf("Serie1Min %d-%d-%d-%d\n\r", Serie1RlMin.Val1, Serie1RlMin.Val2, Serie1RlMin.Val3, Serie1RlMin.Val4);
	printf("Serie1Max %d-%d-%d-%d\n\r", Serie1RlMax.Val1, Serie1RlMax.Val2, Serie1RlMax.Val3, Serie1RlMax.Val4);
	printf("Serie2Min %d-%d-%d-%d\n\r", Serie2RlMin.Val1, Serie2RlMin.Val2, Serie2RlMin.Val3, Serie2RlMin.Val4);
	printf("Serie2Max %d-%d-%d-%d\n\r", Serie2RlMax.Val1, Serie2RlMax.Val2, Serie2RlMax.Val3, Serie2RlMax.Val4);
	printf("\n\r");
	printf("Moyenne\n\r");
	printf("Serie1Moy %d-%d-%d-%d\n\r", Serie1Moy.Val1, Serie1Moy.Val2, Serie1Moy.Val3, Serie1Moy.Val4);
	printf("Serie2Moy %d-%d-%d-%d\n\r", Serie2Moy.Val1, Serie2Moy.Val2, Serie2Moy.Val3, Serie2Moy.Val4);
	printf("\n\r");
	printf("Séries identifiées\n\r");
	printf("Serie1 %d\n\r", Serie1RlMax.Occ);
	printf("Serie2 %d\n\r", Serie2RlMax.Occ);
	printf("\n\r");

	Serie_Equ(&Serie1ThMin, Serie1Moy);		Serie_Sub(&Serie1ThMin, Serie1RlMin);
	Serie_Equ(&Serie1ThMax, Serie1RlMax);	Serie_Sub(&Serie1ThMax, Serie1Moy);
	Serie_Equ(&Serie2ThMin, Serie2Moy);		Serie_Sub(&Serie2ThMin, Serie2RlMin);
	Serie_Equ(&Serie2ThMax, Serie2RlMax);	Serie_Sub(&Serie2ThMax, Serie2Moy);

	printf("Ecart\n\r");
	printf("Serie1ThMin %d-%d-%d-%d\n\r", Serie1ThMin.Val1, Serie1ThMin.Val2, Serie1ThMin.Val3, Serie1ThMin.Val4);
	printf("Serie1ThMax %d-%d-%d-%d\n\r", Serie1ThMax.Val1, Serie1ThMax.Val2, Serie1ThMax.Val3, Serie1ThMax.Val4);
	printf("Serie2ThMin %d-%d-%d-%d\n\r", Serie2ThMin.Val1, Serie2ThMin.Val2, Serie2ThMin.Val3, Serie2ThMin.Val4);
	printf("Serie2ThMax %d-%d-%d-%d\n\r", Serie2ThMax.Val1, Serie2ThMax.Val2, Serie2ThMax.Val3, Serie2ThMax.Val4);
	printf("\n\r");

	Hdl = fopen("data.conf", "w");
	if(Hdl!=NULL)
	{
		fprintf(Hdl, "Serie1=%d-%d,%d-%d,%d-%d,%d-%d\n", Serie1RlMin.Val1, Serie1RlMax.Val1, Serie1RlMin.Val2, Serie1RlMax.Val2, Serie1RlMin.Val3, Serie1RlMax.Val3, Serie1RlMin.Val4, Serie1RlMax.Val4);
		fprintf(Hdl, "Serie2=%d-%d,%d-%d,%d-%d,%d-%d\n", Serie2RlMin.Val1, Serie2RlMax.Val1, Serie2RlMin.Val2, Serie2RlMax.Val2, Serie2RlMin.Val3, Serie2RlMax.Val3, Serie2RlMin.Val4, Serie2RlMax.Val4);
		fclose(Hdl);
		couleur("32");
		printf("Les informations ont été stockées dans le fichier data.conf.\n\r");
	}
	else
	{
		couleur("31");
		printf("Impossible de créer le fichier data.conf.\n\r");
	}
}

void Analyse6()
{
	SIGNAL	*Signal;
	int		i = 0;
	int		okDeb = 0;
	int		okFin = 0;
	int		okiDeb;
	int		okiFin;
	int		v = 0;
	FILE *Hdl;

	int		VerrouDeb1 = 0;
	int		VerrouDeb2 = 0;
	int		VerrouFin1 = 0;
	int		VerrouFin2 = 0;

	int		VerrouDeb1Max = 0;
	int		VerrouDeb1Min = 0;
	int		VerrouDeb2Max = 0;
	int		VerrouDeb2Min = 0;
	int		VerrouFin1Max = 0;
	int		VerrouFin1Min = 0;
	int		VerrouFin2Max = 0;
	int		VerrouFin2Min = 0;


	couleur("37;1");
	printf("--Recherche les verrous-----------------------------------------\n\r");
	couleur("0;37");
	for(Signal=LstSignal; Signal!=NULL; Signal=Signal->Suivant)
	{
		if(LngEmission!=Signal->Occ) continue;
		i++;
		VerrouDeb1 += g_InterruptBuf2[Signal->Deb-2];
		VerrouDeb2 += g_InterruptBuf2[Signal->Deb-1];
		VerrouFin1 += g_InterruptBuf2[Signal->Deb+Signal->Occ*4];
		VerrouFin2 += g_InterruptBuf2[Signal->Deb+Signal->Occ*4+1];
	}

	if(i==0)
	{
		couleur("31");
		printf("Pas de verrous détectés\n\r");
	}

	VerrouDeb1 = VerrouDeb1/i;
	VerrouDeb2 = VerrouDeb2/i;
	VerrouFin1 = VerrouFin1/i;
	VerrouFin2 = VerrouFin2/i;

	VerrouDeb1Max = VerrouDeb1;
	VerrouDeb1Min = VerrouDeb1;
	VerrouDeb2Max = VerrouDeb2;
	VerrouDeb2Min = VerrouDeb2;
	VerrouFin1Max = VerrouFin1;
	VerrouFin1Min = VerrouFin1;
	VerrouFin2Max = VerrouFin2;
	VerrouFin2Min = VerrouFin2;

	for(Signal=LstSignal; Signal!=NULL; Signal=Signal->Suivant)
	{
		if(LngEmission!=Signal->Occ) continue;
		okiDeb=0;
		okiFin=0;
		
		v = g_InterruptBuf2[Signal->Deb-2];
		if(v>VerrouDeb1Max) VerrouDeb1Max = v;
		if(v<VerrouDeb1Min) VerrouDeb1Min = v;
		if((VerrouDeb1*1.4>v)&&(VerrouDeb1/1.4<v)) okiDeb++;

		v = g_InterruptBuf2[Signal->Deb-1];
		if(v>VerrouDeb2Max) VerrouDeb2Max = v;
		if(v<VerrouDeb2Min) VerrouDeb2Min = v;
		if((VerrouDeb2*1.4>v)&&(VerrouDeb1/1.4<v)) okiDeb++;

		v = g_InterruptBuf2[Signal->Deb+Signal->Occ*4];
		if(v>VerrouFin1Max) VerrouFin1Max = v;
		if(v<VerrouFin1Min) VerrouFin1Min = v;
		if((VerrouFin1*1.4>v)&&(VerrouFin1/1.4<v)) okiFin++;

		v = g_InterruptBuf2[Signal->Deb+Signal->Occ*4+1];
		if(v>VerrouFin2Max) VerrouFin2Max = v;
		if(v<VerrouFin2Min) VerrouFin2Min = v;
		if((VerrouFin2*1.4>v)&&(VerrouFin2/1.4<v)) okiFin++;

		if(okiDeb==2) okDeb++;
		if(okiFin==2) okFin++;
	}

	okiDeb = 0;
	okiFin = 0;
	if((NbEmissionOk>=okDeb)&&(NbEmissionOk/1.4<okDeb))	okiDeb = 1;
	if((NbEmissionOk>=okFin)&&(NbEmissionOk/1.4<okFin))	okiFin = 1;

	if(okiDeb == 1)	printf("Verrou Début détecté : %d %d\n\r", VerrouDeb1, VerrouDeb2);
	if(okiFin == 1)	printf("Verrou Fin détecté : %d %d\n\r", VerrouFin1, VerrouFin2);

	Hdl = fopen("data.conf", "a");
	if(Hdl!=NULL)
	{
		if((okiDeb == 1)||(okiFin == 1))
		{
			if(okiDeb == 1)	fprintf(Hdl, "VerrouDeb=%d-%d,%d-%d\n", VerrouDeb1Min, VerrouDeb1Max, VerrouDeb2Min, VerrouDeb2Max);
			if(okiFin == 1)	fprintf(Hdl, "VerrouFin=%d-%d,%d-%d\n", VerrouFin1Min, VerrouFin1Max, VerrouFin2Min, VerrouFin2Max);
		}
		else
		{
			fprintf(Hdl, "Longueur=%d\n", LngEmission);
		}
		fclose(Hdl);
		couleur("32");
		printf("Le fichier data.conf a été mis à jour.\n\r");
	}
	else
	{
		couleur("31");
		printf("Impossible de metter à jour le fichier data.conf.\n\r");
	}

	printf("\n\r");
}
/****************************************************************************************************************/
/* Point d'entrée																								*/
int main (int argc, char** argv)
{
	int i;
	int Start;
	FILE *Hdl;

	int Param_RealTime;
	int Param_FileIn;
	int Param_FileOut;

	float DureeBruit;
	float DureeEmission;


	//******************************************************************************************************************
	//*** Initialisation générale
	Param_RealTime = 0;
	Param_FileIn = 0;
	Param_FileOut = 0;
	g_PinReception = 0;
	g_bModePause = MODE_Pause;

	//******************************************************************************************************************
	//*** Gestion des paramètres
	for (i=1; i < argc; i++)
	{
		switch(argv[i][1])
		{
			case 'P' : 
				g_PinReception = atoi(argv[i]+2);
				break;
			case 'I' :
				Param_FileIn = 1;
				break;
			case 'O' :
				Param_FileOut = 1;
				break;
			case 'R' :
				Param_RealTime = 1;
				break;
		}
	}

	if((g_PinReception==0)&&(Param_FileIn==0))
	{
		AfficheAide();
		return -1;
	}

	//******************************************************************************************************************
	//*** Initialisation WiringPi/Interruption
	if(Param_FileIn==0)
	{
		if(wiringPiSetup() < 0)
		{
			printf("Echec de l'initialisation de la librairie WiringPi : %s\n\r", strerror(errno));
			return -1;
		}
		pinMode(g_PinReception, INPUT);
		if(wiringPiISR(g_PinReception, INT_EDGE_BOTH,  &Radio_Interrupt) < 0)
		{
			printf("Impossible d'accrocher une interruption : %s\n\r", strerror(errno));
			return -1;
		}
	}

	//******************************************************************************************************************
	//*** Mémorisation du bruit puis emission
	if(Param_FileIn==0)
	{
		if(Param_RealTime==1) scheduler_realtime();
		DureeBruit = Memorisation(MODE_Start1)*BUFFER_Coeff;
		DureeEmission = Memorisation(MODE_Start2);
		if(Param_RealTime==1) scheduler_standard();
		Hdl = fopen("data.tmp", "wb");
		fwrite(g_InterruptBuf1, sizeof(unsigned short int), BUFFER1_Size, Hdl);
		fwrite(g_InterruptBuf2, sizeof(unsigned short int), BUFFER2_Size, Hdl);
		fwrite(g_InterruptBub2, sizeof(unsigned char), BUFFER2_Size, Hdl);
		fwrite(g_InterruptBut2, sizeof(unsigned char), BUFFER2_Size, Hdl);
		fwrite(&DureeBruit, sizeof(float), 1, Hdl);
		fwrite(&DureeEmission, sizeof(float), 1, Hdl);
		fclose(Hdl);
	}
	else
	{
		Hdl = fopen("data.tmp", "rb");
		if(Hdl!=NULL)
		{
			fread(g_InterruptBuf1, sizeof(unsigned short int), BUFFER1_Size, Hdl);
			fread(g_InterruptBuf2, sizeof(unsigned short int), BUFFER2_Size, Hdl);
			fread(g_InterruptBub2, sizeof(unsigned char), BUFFER2_Size, Hdl);
			fread(g_InterruptBut2, sizeof(unsigned char), BUFFER2_Size, Hdl);
			fread(&DureeBruit, sizeof(float), 1, Hdl);
			fread(&DureeEmission, sizeof(float), 1, Hdl);
			fclose(Hdl);
		}
	}

	//******************************************************************************************************************
	//*** Première analyse : Rapport bruit/émission
	Analyse1(DureeBruit, DureeEmission);

	//******************************************************************************************************************
	//*** Deuxième analyse : Recherche d'une constance de temps sur des séries de 4 mesures
	Start = Analyse2();

	//******************************************************************************************************************
	//*** Troisième analyse : Nombre et longueur des émissions
	Analyse3(Start);

	//******************************************************************************************************************
	//*** Quatrième analyse : Identifier les différentes séries de 4 mesures
	Analyse4();

	//******************************************************************************************************************
	//*** Cinquième analyse : Identifier les mini/maxi sur les 2 séries de 4 mesures
	Analyse5(1.4);

	//******************************************************************************************************************
	//*** Sixème analyse : Identifier les verrous
	Analyse6();

	//******************************************************************************************************************
	//*** Sauvegarde dns un fichier texte
	couleur("37");
	if(Param_FileOut==1)
	{
		Hdl = fopen("data.txt", "w");
		if(Hdl!=NULL)
		{
			for(i=0; i<BUFFER2_Size; i++)
			{
				fprintf(Hdl, "%d;%d;%d\n", g_InterruptBuf2[i], g_InterruptBub2[i], g_InterruptBut2[i]);
			}
			fclose(Hdl);
			printf("Fichier texte généré.\r\n");
		}
	}

	//******************************************************************************************************************
	//*** C'est fini
	printf("FIN\n\r");
	return 0;
}  // main  