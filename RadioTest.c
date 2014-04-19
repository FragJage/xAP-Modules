//Ne marche pas avec longueur du signal, uniquement avec verrou de fin
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <wiringPi.h>
#include <sched.h>
#include <errno.h>
#include <string.h>

#define couleur(param) printf("\033[%sm",param)
/*   param devant être un const char *, vide (identique à "0") ou formé
     d'une où plusieurs valeurs séparées par des ; parmi
         0  réinitialisation         1  haute intensité (des caractères)
         5  clignotement             7  video inversé
         30, 31, 32, 33, 34, 35, 36, 37 couleur des caractères
         40, 41, 42, 43, 44, 45, 46, 47 couleur du fond
            les couleurs, suivant la logique RGB, étant respectivement
               noir, rouge, vert, jaune, bleu, magenta, cyan et blanc */

#define FALSE	0
#define TRUE	1
typedef int    BOOL;

typedef struct _Serie4
{
	unsigned int	ValMin[4];
	unsigned int	ValMax[4];
	int				Longueur;
} SERIE4;

SERIE4 g_Serie1;
SERIE4 g_Serie2;
SERIE4 g_VerrouDeb;
SERIE4 g_VerrouFin;
int g_LongueurSerie;
int g_PinReception;
struct timeval g_InterruptTime;
volatile int g_bStart;
volatile long long g_Stream=0;
volatile BOOL g_bStreamOK=FALSE;

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

void Radio_Interrupt() //BOOL bFrontMontant)
{
	struct timeval t;
	unsigned long micros;
	unsigned long valeur;
	int max;

	volatile static long long s_Stream=0;
	volatile static BOOL s_bDeb  = TRUE;
	volatile static BOOL s_bBit0 = FALSE;
	volatile static BOOL s_bBit1 = FALSE;
	volatile static BOOL s_bFin  = FALSE;
	volatile static int s_Pos    = 0;
	
	if(g_bStart==FALSE) return;

	//*** Longueur du front précédent en µs
	gettimeofday(&t, NULL);
    if (t.tv_sec > g_InterruptTime.tv_sec) micros = 1000000L; else micros = 0;
    micros += (t.tv_usec - g_InterruptTime.tv_usec);
	if(micros < 50) return; //Parasites

	//*** Mémoriser le début du front suivant
	g_InterruptTime.tv_sec = t.tv_sec;
	g_InterruptTime.tv_usec = t.tv_usec;

	//*** Controle du verrou début
	if((s_bDeb)&&(s_Pos==g_VerrouDeb.Longueur))
	{
		s_Pos   = 0;
		s_Stream=0;
		s_bDeb  = FALSE;
		s_bBit0 = TRUE;
		s_bBit1 = TRUE;
		s_bFin  = TRUE;
	}
	if(s_bDeb)
	{
		if((micros<g_VerrouDeb.ValMin[s_Pos])||(micros>g_VerrouDeb.ValMax[s_Pos]))
			s_Pos = 0;
		else
			s_Pos++;
		return;
	}

	//*** Détection Bit0
	if(s_bBit0)
	{
		if((micros>g_Serie1.ValMin[s_Pos])&&(micros<g_Serie1.ValMax[s_Pos]))
		{
			if((s_Pos+1)==g_Serie1.Longueur)
			{
				s_Pos   = 0;
				s_bBit0 = TRUE;
				s_bBit1 = TRUE;
				s_bFin  = TRUE;
				s_Stream <<= 1;
				return;
			}
		}
		else
		{
			s_bBit0 = FALSE;
		}
	}

	//*** Détection Bit1
	if(s_bBit1)
	{
		if((micros>g_Serie2.ValMin[s_Pos])&&(micros<g_Serie2.ValMax[s_Pos]))
		{
			if((s_Pos+1)==g_Serie2.Longueur)
			{
				s_Pos   = 0;
				s_bBit0 = TRUE;
				s_bBit1 = TRUE;
				s_bFin  = TRUE;
				s_Stream <<= 1;
				s_Stream |= 1;
				return;
			}
		}
		else
		{
			s_bBit1 = FALSE;
		}
	}

	//*** Détection Verrou Fin
	if(s_bFin)
	{
		if((micros>g_VerrouFin.ValMin[s_Pos])&&(micros<g_VerrouFin.ValMax[s_Pos]))
		{
			if((s_Pos+1)==g_VerrouFin.Longueur)
			{
				s_Pos   = 0;
				s_bDeb = TRUE;
				g_Stream = s_Stream;
				g_bStreamOK = TRUE;
				return;
			}
		}
		else
		{
			s_bFin = FALSE;
		}
	}

	s_Pos++;

	if(!(s_bBit0|s_bBit1|s_bFin))
	{
		/*
		for(max=0; max<DebugMemoPos; max++)
		{
			Flog_Ecrire("%d", DebugMemoTps[max]);
		}
		*/
		//Flog_Ecrire("Echec sur Tps %d Pos %d", micros, s_Pos-1);
		s_Pos   = 0;
		s_bDeb = TRUE;
	}
}

void AfficheAide()
{
	couleur("37;1");
	printf("RadioTest\n\r");
	couleur("0;37");
	printf("	Test le fichier de config produit par RadioAnalyse pour tenter\n\r");
	printf("	d'identifier un protocole pour le service xAP-Radio.\n\r");
	couleur("37;1");
	printf("Synopsis\n\r");
	couleur("0;37");
	printf("	RadioTest -Pn -R\n\r");
	couleur("37;1");
	printf("Paramètres\n\r");
	couleur("0;37");
	printf("	-Pn : Pin sur lequel est connecté le récepteur radio.\n\r");
	printf("	-R : Bascule en mode temps réel.\n\r");
	printf("\n\r");
}

void ParseVal(SERIE4 *Serie, char *Val)
{
	char *Ptr;
	char *pVirgule;
	char *pTiret;

	Serie->Longueur = 0;

	Ptr = Val;
	Serie->ValMin[0] = atoi(Ptr);
	Ptr = strchr(Ptr, '-');
	if(Ptr==NULL) return;
	Serie->ValMax[0] = atoi(++Ptr);
	Serie->Longueur++;

	Ptr = strchr(Ptr, ',');
	if(Ptr==NULL) return;
	Serie->ValMin[1] = atoi(++Ptr);
	Ptr = strchr(Ptr, '-');
	if(Ptr==NULL) return;
	Serie->ValMax[1] = atoi(++Ptr);
	Serie->Longueur++;

	Ptr = strchr(Ptr, ',');
	if(Ptr==NULL) return;
	Serie->ValMin[2] = atoi(++Ptr);
	Ptr = strchr(Ptr, '-');
	if(Ptr==NULL) return;
	Serie->ValMax[2] = atoi(++Ptr);
	Serie->Longueur++;

	Ptr = strchr(Ptr, ',');
	if(Ptr==NULL) return;
	Serie->ValMin[3] = atoi(++Ptr);
	Ptr = strchr(Ptr, '-');
	if(Ptr==NULL) return;
	Serie->ValMax[3] = atoi(++Ptr);
	Serie->Longueur++;
}

/*
void DebugSerie(SERIE4 Serie)
{
	printf("Longueur : %d\n\r", Serie.Longueur);
	printf("%d-%d, %d-%d", Serie.ValMin[0], Serie.ValMax[0], Serie.ValMin[1], Serie.ValMax[1]);
	if(Serie.Longueur < 3) return;
	printf(", %d-%d, %d-%d", Serie.ValMin[2], Serie.ValMax[2], Serie.ValMin[3], Serie.ValMax[3]);
}
*/

long long getIdBouton(char *Msg)
{
	int i;
	long long btn=0;


	printf("%s\n",Msg);
	g_bStart = TRUE;
	for(i=0; i<5000; i++)
	{
		delay(1);
		if(g_bStreamOK)	
		{
			btn = g_Stream;
			printf("Détecté : %lld", btn);
			g_bStreamOK = FALSE;
			if(i<4990) i = 4990;
		}
	}
	g_bStart = FALSE;

	if(btn==0) printf("Non détecté !");
	else
		printf("Détecté : %lld", btn);

	printf("\n");
	return btn;
}

void getRangeBit2(int BitDeb1, int BitFin1, int BitDeb2, int BitFin2, int *BitDeb, int *BitFin)
{
	int i, Deb, Fin;
	int Max;
	BOOL b=FALSE;


	for(i=1; i<65; i++)
	{
		if((i>=BitDeb1)&&(i<=BitFin1)||(i>=BitDeb2)&&(i<=BitFin2))
		{
			if(b) 
			{
				Fin = i-1;
				b=FALSE;
				if(Fin-Deb>Max)
				{
					Max = Fin-Deb;
					*BitDeb = Deb;
					*BitFin = Fin;
				}
			}
		}
		else
		{
			if(!b)
			{
				Deb = i;
				b=TRUE;
			}
		}
	}

	if((b)&&(64-Deb>Max))
	{
		Max = 64-Deb;
		*BitDeb = Deb;
		*BitFin = 64;
	}

}

void getRangeBit(unsigned long long Id1, unsigned long long Id2, int *BitDeb, int *BitFin)
{
	int i;
	BOOL b=FALSE;

	*BitDeb = -1;
	*BitFin = -1;

	for(i=0; i<64; i++)
	{
		if( (Id1&1) != (Id2&1) )
		{
			//printf("|");
			b = TRUE;
			if(*BitDeb==-1) *BitDeb = i;
		}
		else
		{
			//printf("=");
			if(b==TRUE) { *BitFin = i-1; b=FALSE; }
		}
		Id1 = Id1>>1;
		Id2 = Id2>>1;
	}
	(*BitDeb)++;
	(*BitFin)++;
}

void affichage_binaire(unsigned long long var)
{ 
	int i;
	unsigned long long mask;


	mask = 1ULL<<63;
	printf("En binaire : ", var);
	for (i=0; i<64; i++)
	{
		printf("%d", ((var&mask)>0)?1:0);
		mask = mask>>1;
	}
	printf("\n");
}

/****************************************************************************************************************/
/* Point d'entrée																								*/
int main (int argc, char** argv)
{
	int i;
	FILE *Hdl;
	char Ligne[256];
	char *Val;
	int Param_RealTime;
	int BitDebNo,BitFinNo,BitDebON,BitFinON,BitDebId,BitFinId;

	long long i_BtnMini;
	long long i_BtnMaxi;
	long long i_BtnON;
	long long i_BtnOFF;


	//******************************************************************************************************************
	//*** Initialisation générale
	Param_RealTime	= 0;
	g_PinReception	= 0;
	g_bStart		= FALSE;

	//******************************************************************************************************************
	//*** Gestion des paramètres
	for (i=1; i < argc; i++)
	{
		switch(argv[i][1])
		{
			case 'P' : 
				g_PinReception = atoi(argv[i]+2);
				break;
			case 'R' :
				Param_RealTime = 1;
				break;
		}
	}

	if(g_PinReception==0)
	{
		AfficheAide();
		return -1;
	}

	//******************************************************************************************************************
	//*** Lecture du fichier conf
	Hdl = fopen("data.conf", "r");
	if(Hdl==NULL)
	{
		printf("Impossible d'ouvrir le fichier data.conf.\n\rLe fichier est normalement créé par RadioAnalyse.\n\r");
		return -1;
	}
	while(!feof(Hdl))
	{
		fgets(Ligne, 256, Hdl);
		Val = strchr(Ligne, '=');
		if(Val!=NULL)
		{
			*Val = '\0';
			Val++;

			if(strcmp(Ligne, "Serie1")==0) ParseVal(&g_Serie1, Val);
			if(strcmp(Ligne, "Serie2")==0) ParseVal(&g_Serie2, Val);
			if(strcmp(Ligne, "VerrouDeb")==0) ParseVal(&g_VerrouDeb, Val);
			if(strcmp(Ligne, "VerrouFin")==0) ParseVal(&g_VerrouFin, Val);
			if(strcmp(Ligne, "Longueur")==0) g_LongueurSerie = atoi(Val);
		}
	}
	fclose(Hdl);

	//******************************************************************************************************************
	//*** Initialisation WiringPi/Interruption
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

	//******************************************************************************************************************
	//*** Lecture OFF/ON et n° de bouton
	gettimeofday(&g_InterruptTime, NULL);
	if(Param_RealTime==1) scheduler_realtime();
	/*
	i_BtnMini = getIdBouton("Appuyer sur le bouton n° mini...");
	i_BtnMaxi = getIdBouton("Appuyer sur le bouton n° maxi...");
	i_BtnON   = getIdBouton("Appuyer sur le bouton n°1 ON...");
	i_BtnOFF  = getIdBouton("Appuyer sur un bouton n°1 OFF...");
	*/
	i_BtnMini = 673829008ULL;
	i_BtnMaxi = 673829023ULL;
	i_BtnON   = 673829011ULL;
	i_BtnOFF  = 673828995ULL;
	if(Param_RealTime==1) scheduler_standard();

	//******************************************************************************************************************
	//*** Identification bit OFF/ON et n° de bouton
	getRangeBit(i_BtnMini, i_BtnMaxi, &BitDebNo, &BitFinNo);
	getRangeBit(i_BtnON, i_BtnOFF, &BitDebON, &BitFinON);
	getRangeBit2(BitDebNo, BitFinNo, BitDebON, BitFinON, &BitDebId, &BitFinId);

	printf("BtnMini : %lld\n", i_BtnMini); affichage_binaire(i_BtnMini);
	printf("BtnMaxi : %lld\n", i_BtnMaxi); affichage_binaire(i_BtnMaxi);
	printf("BtnON : %lld\n",   i_BtnON);   affichage_binaire(i_BtnON);
	printf("BtnOFF : %lld\n",  i_BtnOFF);  affichage_binaire(i_BtnOFF);

	printf("Bits d'identification du bouton %d-%d\n\r", BitDebNo, BitFinNo);
	printf("Bits d'identification de l'état %d-%d\n\r", BitDebON, BitFinON);
	printf("Bits d'identification de la télécommande %d-%d\n\r", BitDebId, BitFinId);

	Hdl = fopen("data2.conf", "w");
	if(Hdl!=NULL)
	{
		fprintf(Hdl, "Serie1=%d-%d,%d-%d,%d-%d,%d-%d\n", g_Serie1.ValMin[0], g_Serie1.ValMax[0], g_Serie1.ValMin[1], g_Serie1.ValMax[1], g_Serie1.ValMin[2], g_Serie1.ValMax[2], g_Serie1.ValMin[3], g_Serie1.ValMax[3]);
		fprintf(Hdl, "Serie2=%d-%d,%d-%d,%d-%d,%d-%d\n", g_Serie2.ValMin[0], g_Serie2.ValMax[0], g_Serie2.ValMin[1], g_Serie2.ValMax[1], g_Serie2.ValMin[2], g_Serie2.ValMax[2], g_Serie2.ValMin[3], g_Serie2.ValMax[3]);
		fprintf(Hdl, "VerrouDeb=%d-%d,%d-%d\n", g_VerrouDeb.ValMin[0], g_VerrouDeb.ValMax[0], g_VerrouDeb.ValMin[1], g_VerrouDeb.ValMax[1]);
		fprintf(Hdl, "VerrouFin=%d-%d,%d-%d\n", g_VerrouFin.ValMin[0], g_VerrouFin.ValMax[0], g_VerrouFin.ValMin[1], g_VerrouFin.ValMax[1]);
		fprintf(Hdl, "BitsIdent=%d-%d\n", BitDebId, BitFinId);
		fprintf(Hdl, "BitsBouton=%d-%d\n", BitDebNo, BitFinNo);
		fprintf(Hdl, "BitsEtat=%d-%d\n", BitDebON, BitFinON);
		fclose(Hdl);
		couleur("32");
		printf("Les informations ont été stockées dans le fichier data2.conf.\n\r");
	}
	else
	{
		couleur("31");
		printf("Impossible de créer le fichier data2.conf.\n\r");
	}


	//******************************************************************************************************************
	//*** C'est fini
	printf("FIN\n\r");
	return 0;
}  // main  