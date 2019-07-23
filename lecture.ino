#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <SoftwareSerial.h>

#define echo_USB

SoftwareSerial serie (8, 9);

byte inByte = 0;        // caractère entrant téléinfo
char buffteleinfo[21] = "";
byte bufflen = 0;
byte num_abo = 0;
byte type_mono_tri[2] = {0,0};
uint8_t presence_teleinfo = 0;  // si signal teleinfo présent
byte presence_PAP = 0;          // si PAP présent
boolean cpt2_present = true;    // mémorisation de la présence du compteur 2
boolean compteursluOK = false;  // pour autoriser l'écriture de valeur sur la SD

int ReceptionOctet = 0; // variable de stockage des octets reçus par port série
unsigned int ReceptionNombre = 0; // variable de calcul du nombre reçu par port série
byte reg_horloge = 1;
boolean mem_reg_horloge = false;
const uint8_t val_max[6] = {24,59,59,31,12,99};

// declarations Teleinfo
unsigned int papp = 0;  // Puissance apparente, VA

uint8_t IINST[2][3] = {{0,0,0},{0,0,0}}; // Intensité Instantanée Phase 1, A  (intensité efficace instantanée) ou 1 phase en monophasé

unsigned long INDEX1 = 0;    // Index option Tempo - Heures Creuses Jours Bleus, Wh
unsigned long INDEX2 = 0;    // Index option Tempo - Heures Pleines Jours Bleus, Wh
unsigned long INDEX3 = 0;    // Index option Tempo - Heures Creuses Jours Blancs, Wh
unsigned long INDEX4 = 0;    // Index option Tempo - Heures Pleines Jours Blancs, Wh
unsigned long INDEX5 = 0;    // Index option Tempo - Heures Creuses Jours Rouges, Wh
unsigned long INDEX6 = 0;    // Index option Tempo - Heures Pleines Jours Rouges, Wh

// compteur 2 (solaire configuré en tarif BASE par ERDF)
unsigned long cpt2index = 0; // Index option Base compteur production solaire, Wh
unsigned int cpt2puissance = 0;  // Puissance apparente compteur production solaire, VA

#define debtrame 0x02
#define debligne 0x0A
#define finligne 0x0D

// *************** déclaration carte micro SD ******************
const byte chipSelect = 4;

// *************** déclaration activation compteur 1 ou 2 ******
#define LEC_CPT1 5  // lecture compteur 1
#define LEC_CPT2 6  // lecture compteur 2

byte verif_cpt_lu = 0;


byte compteur_actif = 1;  // numero du compteur en cours de lecture
byte donnee_ok_cpt[2] = {0,0};  // pour vérifier que les donnees sont bien en memoire avant ecriture dans fichier
byte donnee_ok_cpt_ph[2] = {0,0};

// *************** variables RTC ************************************
byte minute, heure, seconde, jour, mois;
unsigned int annee;
char mois_jour[7];
RTC_DS1307 RTC;


void setup () 
{
    // initialisation du port 0-1 lecture Téléinfo
    Serial.begin(1200);
    UCSR0C = B00100100; // parité paire E, 7 bits de données

    serie.begin(2400);

    serie.println(F("Starting..."));
    serie.flush();

    serie.listen();

    // initialisation des sorties selection compteur
    pinMode(LEC_CPT1, OUTPUT);
    pinMode(LEC_CPT2, OUTPUT);
    digitalWrite(LEC_CPT1, HIGH);
    digitalWrite(LEC_CPT2, LOW);

    // Initialisation de la carte microSD
    if (!SD.begin(chipSelect)) {
        serie.println(F("Erreur carte"));
        return;
    }

    // Initialisation RTC
    Wire.begin();
    RTC.begin();

    if (!RTC.isrunning()) {
        serie.println(F("Erreur RTC"));
    }

    DateTime now = RTC.now();
    annee = now.year();
    mois = now.month();
    jour = now.day();
    heure = now.hour();
    minute = now.minute();

    serie.println(F("Started!"));
}

void loop ()
{
    // Réglage de l'horloge si nécessaire
    if (!(RTC.isrunning()) && (reg_horloge < 7)) {
        digitalWrite(LEC_CPT1, LOW);

        if (!mem_reg_horloge) {
            switch (reg_horloge) { // debut de la structure
            case 1: serie.print (F("Heure: "));
                break;
            case 2: serie.print (F("Minute: "));
                break;
            case 3: serie.print (F("Seconde: "));
                break;
            case 4: serie.print (F("Jour: "));
                break;
            case 5: serie.print (F("Mois: "));
                break;
            case 6: serie.print (F("Annee 20xx: "));
                break;
            }
            mem_reg_horloge = true;
        }

        if (serie.available()) {

            //---- lecture du nombre reçu
            while (serie.available()) {
                inByte = serie.read();
                if (((inByte > 47) && (inByte < 58)) || (inByte == 13 )) {
                    ReceptionOctet = inByte - '0';
                    if ((ReceptionOctet >= 0) && (ReceptionOctet <= 9)) ReceptionNombre = (ReceptionNombre*10) + ReceptionOctet;
                    // si valeur reçue correspond à un chiffre on calcule nombre
                }
                else presence_teleinfo = -1;
            }

            if (inByte == 13){
                if ((ReceptionNombre > val_max[reg_horloge-1]) || (ReceptionNombre == -1)) {
                    serie.println(F("Erreur"));
                    ReceptionNombre = 0;
                    mem_reg_horloge = true;
                }
                else {
                    switch (reg_horloge) { // debut de la structure
                    case 1: heure = ReceptionNombre;
                        break;
                    case 2: minute = ReceptionNombre;
                        break;
                    case 3: seconde = ReceptionNombre;
                        break;
                    case 4: jour = ReceptionNombre;
                        break;
                    case 5: mois = ReceptionNombre;
                        break;
                    case 6: annee = 2000 + ReceptionNombre;
                        break;
                    }

                    mem_reg_horloge = false;
                    ReceptionNombre = 0;
                    ++reg_horloge;

                    if (reg_horloge > 6) {

                        // Mise à jour de la RTC
                        Wire.beginTransmission(104); // 104 is DS1307 device address (0x68)
                        Wire.write(bin2bcd(0)); // start at register 0

                        Wire.write(bin2bcd(seconde)); //Send seconds as BCD
                        Wire.write(bin2bcd(minute)); //Send minutes as BCD
                        Wire.write(bin2bcd(heure)); //Send hours as BCD
                        Wire.write(bin2bcd(0)); // Jour de la semaine (inutilisé)
                        Wire.write(bin2bcd(jour)); //Send day as BCD
                        Wire.write(bin2bcd(mois)); //Send month as BCD
                        Wire.write(bin2bcd(annee % 1000)); //Send year as BCD

                        Wire.endTransmission();

                        serie.println(F("Reglage heure OK"));
                        digitalWrite(LEC_CPT1, HIGH);
                    }
                }
            }
        }
    }
    else {
        DateTime now = RTC.now();

        seconde = now.second();
        minute = now.minute();
        heure = now.hour();
        jour = now.day();
        mois = now.month();
        annee = now.year();

        // Enregistrer (une fois par minute)
        static bool doitSauvegarder = true;

        if (seconde == 1) {
            if (doitSauvegarder && compteursluOK) {
                enregistrer();
                doitSauvegarder = false;
            }
        }
        else {
            doitSauvegarder = true;
        }


        if ((donnee_ok_cpt[0] == verif_cpt_lu) and (cpt2_present)) {
            if ((type_mono_tri[0] == 1) && (donnee_ok_cpt_ph[0] == B10000001)) bascule_compteur();
            else if ((type_mono_tri[0] == 3) && (donnee_ok_cpt_ph[0] == B10000111)) bascule_compteur();
        }
        else if ((donnee_ok_cpt[1] == B00000001) or ((compteur_actif == 2) and (!cpt2_present))) {
            if ((type_mono_tri[1] == 1) && (donnee_ok_cpt_ph[1] == B10000001)) bascule_compteur();
            else if ((type_mono_tri[1] == 3) && (donnee_ok_cpt_ph[1] == B10000111)) bascule_compteur();
            compteursluOK = true;
        }

        if (compteur_actif == 2) {
            if (presence_teleinfo > 200) {
                cpt2_present = false;
                compteursluOK = true;
                bascule_compteur();
            }
            else cpt2_present = true;
        }

        // Lecture des données
        read_teleinfo();
    }
}




///////////////////////////////////////////////////////////////////
// Analyse de la ligne de Teleinfo
///////////////////////////////////////////////////////////////////
void traitbuf_cpt(char *buff, uint8_t len)
{
    char optarif[4]= "";    // Option tarifaire choisie: BASE => Option Base, HC.. => Option Heures Creuses,
    // EJP. => Option EJP, BBRx => Option Tempo [x selon contacts auxiliaires]

    if (compteur_actif == 1){

        if (num_abo == 0){ // détermine le type d'abonnement

            if (strncmp("OPTARIF ", &buff[1] , 8)==0){
                strncpy(optarif, &buff[9], 3);
                optarif[3]='\0';

                if (strcmp("BAS", optarif)==0) {
                    num_abo = 1;
                    verif_cpt_lu = B00000001;
                }
                else if (strcmp("HC.", optarif)==0) {
                    num_abo = 2;
                    verif_cpt_lu = B00000011;
                }
                else if (strcmp("EJP", optarif)==0) {
                    num_abo = 3;
                    verif_cpt_lu = B00000011;
                }
                else if (strcmp("BBR", optarif)==0) {
                    num_abo = 4;
                    verif_cpt_lu = B00111111;
                }

                serie.print(F("- type ABO: "));
                serie.println(optarif);
            }
        }
        else {

            if (num_abo == 1){
                if (strncmp("BASE ", &buff[1] , 5)==0){
                    INDEX1 = atol(&buff[6]);
                    donnee_ok_cpt[0] = donnee_ok_cpt[0] | B00000001;
                }
            }

            else if (num_abo == 2){
                if (strncmp("HCHP ", &buff[1] , 5)==0){
                    INDEX1 = atol(&buff[6]);
                    donnee_ok_cpt[0] = donnee_ok_cpt[0] | B00000001;
                }
                else if (strncmp("HCHC ", &buff[1] , 5)==0){
                    INDEX2 = atol(&buff[6]);
                    donnee_ok_cpt[0] = donnee_ok_cpt[0] | B00000010;
                }
            }

            else if (num_abo == 3){
                if (strncmp("EJPHN ", &buff[1] , 6)==0){
                    INDEX1 = atol(&buff[7]);
                    donnee_ok_cpt[0] = donnee_ok_cpt[0] | B00000001;
                }
                else if (strncmp("EJPHPM ", &buff[1] , 7)==0){
                    INDEX2 = atol(&buff[8]);
                    donnee_ok_cpt[0] = donnee_ok_cpt[0] | B00000010;
                }
            }

            else if (num_abo == 4){
                if (strncmp("BBRHCJB ", &buff[1] , 8)==0){
                    INDEX1 = atol(&buff[9]);
                    donnee_ok_cpt[0] = donnee_ok_cpt[0] | B00000001;
                }
                else if (strncmp("BBRHPJB ", &buff[1] , 8)==0){
                    INDEX2 = atol(&buff[9]);
                    donnee_ok_cpt[0] = donnee_ok_cpt[0] | B00000010;
                }
                else if (strncmp("BBRHCJW ", &buff[1] , 8)==0){
                    INDEX3 = atol(&buff[9]);
                    donnee_ok_cpt[0] = donnee_ok_cpt[0] | B00000100;
                }
                else if (strncmp("BBRHPJW ", &buff[1] , 8)==0){
                    INDEX4 = atol(&buff[9]);
                    donnee_ok_cpt[0] = donnee_ok_cpt[0] | B00001000;
                }
                else if (strncmp("BBRHCJR ", &buff[1] , 8)==0){
                    INDEX5 = atol(&buff[9]);
                    donnee_ok_cpt[0] = donnee_ok_cpt[0] | B00010000;
                }
                else if (strncmp("BBRHPJR ", &buff[1] , 8)==0){
                    INDEX6 = atol(&buff[9]);
                    donnee_ok_cpt[0] = donnee_ok_cpt[0] | B00100000;
                }
            }
        }

        if (type_mono_tri[0] == 0){ // détermine le type mono ou triphasé

            if (strncmp("IINST", &buff[1] , 5)==0) {
                if (strncmp("IINST ", &buff[1] , 6)==0) type_mono_tri[0]  = 1; // monophasé
                else if (strncmp("IINST1 ", &buff[1] , 7)==0) type_mono_tri[0] = 3; // triphasé
                else if (strncmp("IINST2 ", &buff[1] , 7)==0) type_mono_tri[0] = 3; // triphasé
                else if (strncmp("IINST3 ", &buff[1] , 7)==0) type_mono_tri[0] = 3; // triphasé

                serie.print(F("- type MONO ou TRI: "));
                if (type_mono_tri[0] == 1) serie.println(F("Monophase"));
                if (type_mono_tri[0] == 3) serie.println(F("Triphase"));
            }
        }
        else
        {
            if (type_mono_tri[0] == 1) {
                if (strncmp("IINST ", &buff[1] , 6)==0){
                    IINST[0][0] = atol(&buff[7]);
                    donnee_ok_cpt_ph[0] = donnee_ok_cpt_ph[0] | B00000001;
                    presence_PAP++;
                }
            }
            else if (type_mono_tri[0] == 3) {
                if (strncmp("IINST1 ", &buff[1] , 7)==0){
                    IINST[0][0] = atol(&buff[8]);
                    donnee_ok_cpt_ph[0] = donnee_ok_cpt_ph[0] | B00000001;
                }
                else if (strncmp("IINST2 ", &buff[1] , 7)==0){
                    IINST[0][1] = atol(&buff[8]);
                    donnee_ok_cpt_ph[0] = donnee_ok_cpt_ph[0] | B00000010;
                }
                else if (strncmp("IINST3 ", &buff[1] , 7)==0){
                    IINST[0][2] = atol(&buff[8]);
                    donnee_ok_cpt_ph[0] = donnee_ok_cpt_ph[0] | B00000100;
                    presence_PAP++;
                }
            }
        }

        if (strncmp("PAPP ", &buff[1] , 5)==0){
            papp = atol(&buff[6]);
            donnee_ok_cpt_ph[0] = donnee_ok_cpt_ph[0] | B10000000;
        }
        // si pas d'index puissance apparente (PAP) calcul de la puissance
        if ((presence_PAP > 2) && !(donnee_ok_cpt_ph[0] & B10000000)) {
            if (type_mono_tri[0] == 1) papp = IINST[0][0] * 240;
            if (type_mono_tri[0] == 3) papp = IINST[0][0] * 240 + IINST[0][1] * 240 + IINST[0][2] * 240;
            donnee_ok_cpt_ph[0] = donnee_ok_cpt_ph[0] | B10000000;
        }

    }
    if (compteur_actif == 2){
        if (strncmp("BASE ", &buff[1] , 5)==0){
            cpt2index = atol(&buff[6]);
            donnee_ok_cpt[1] = donnee_ok_cpt[1] | B00000001;
        }
        if (type_mono_tri[1] == 0){ // détermine le type mono ou triphasé du compteur 2

            if (strncmp("IINST", &buff[1] , 5)==0) {
                if (strncmp("IINST ", &buff[1] , 6)==0) type_mono_tri[1]  = 1; // monophasé
                else if (strncmp("IINST1 ", &buff[1] , 7)==0) type_mono_tri[1] = 3; // triphasé
                else if (strncmp("IINST2 ", &buff[1] , 7)==0) type_mono_tri[1] = 3; // triphasé
                else if (strncmp("IINST3 ", &buff[1] , 7)==0) type_mono_tri[1] = 3; // triphasé

                serie.print(F("- type MONO ou TRI: "));
                if (type_mono_tri[1] == 1) serie.println(F("Monophase"));
                if (type_mono_tri[1] == 3) serie.println(F("Triphase"));
            }
        }
        else
        {
            if (type_mono_tri[1] == 1) {
                if (strncmp("IINST ", &buff[1] , 6)==0){
                    IINST[1][0] = atol(&buff[7]);
                    donnee_ok_cpt_ph[1] = donnee_ok_cpt_ph[1] | B00000001;
                    presence_PAP++;
                }
            }
            else if (type_mono_tri[1] == 3) {
                if (strncmp("IINST1 ", &buff[1] , 7)==0){
                    IINST[1][0] = atol(&buff[8]);
                    donnee_ok_cpt_ph[1] = donnee_ok_cpt_ph[1] | B00000001;
                }
                else if (strncmp("IINST2 ", &buff[1] , 7)==0){
                    IINST[1][1] = atol(&buff[8]);
                    donnee_ok_cpt_ph[1] = donnee_ok_cpt_ph[1] | B00000010;
                }
                else if (strncmp("IINST3 ", &buff[1] , 7)==0){
                    IINST[1][2] = atol(&buff[8]);
                    donnee_ok_cpt_ph[1] = donnee_ok_cpt_ph[1] | B00000100;
                    presence_PAP++;
                }
            }
        }
        // si pas d'index puissance apparente (PAP) calcul de la puissance
        if ((presence_PAP > 2) && !(donnee_ok_cpt_ph[1] & B10000000)) {
            if (type_mono_tri[1] == 1) cpt2puissance = IINST[1][0] * 240;
            if (type_mono_tri[1] == 3) cpt2puissance = IINST[1][0] * 240 + IINST[1][1] * 240 + IINST[1][2] * 240;
            donnee_ok_cpt_ph[1] = donnee_ok_cpt_ph[1] | B10000000;
        }
    }
}

///////////////////////////////////////////////////////////////////
// Changement de lecture de compteur
///////////////////////////////////////////////////////////////////
void bascule_compteur()
{
    if (compteur_actif == 1)
    {
        digitalWrite(LEC_CPT1, LOW);
        digitalWrite(LEC_CPT2, HIGH);
        compteur_actif = 2;
    }
    else
    {
        digitalWrite(LEC_CPT1, HIGH);
        digitalWrite(LEC_CPT2, LOW);
        compteur_actif = 1;
    }

    donnee_ok_cpt[0] = B00000000;
    donnee_ok_cpt[1] = B00000000;
    donnee_ok_cpt_ph[0] = B00000000;
    donnee_ok_cpt_ph[1] = B00000000;
    presence_PAP = 0;
    presence_teleinfo = 0;
    bufflen=0;

    // memset(buffteleinfo,0,21);  // vide le buffer lorsque l'on change de compteur
    Serial.flush();
    buffteleinfo[0]='\0';
    delay(500);
}

///////////////////////////////////////////////////////////////////
// Lecture trame teleinfo (ligne par ligne)
/////////////////////////////////////////////////////////////////// 
void read_teleinfo()
{
    presence_teleinfo++;
    if (presence_teleinfo > 250) presence_teleinfo = 0;

    // si une donnée est dispo sur le port série
    if (Serial.available() > 0)
    {
        presence_teleinfo = 0;
        // recupère le caractère dispo
        inByte = Serial.read();

#ifdef echo_USB    
        serie.print(char(inByte));  // echo des trames sur l'USB (choisir entre les messages ou l'echo des trames)
#endif

        if (inByte == debtrame) bufflen = 0; // test le début de trame
        if (inByte == debligne) // test si c'est le caractère de début de ligne
        {
            bufflen = 0;
        }
        buffteleinfo[bufflen] = inByte;
        bufflen++;
        if (bufflen > 21)bufflen=0;

        if (inByte == finligne && bufflen > 5) // si Fin de ligne trouvée
        {
            // Calcul du checksum
            char checksum = 0;
            for (int i=1; i<bufflen-3; i++) {
                checksum += buffteleinfo[i];
            }
            checksum = (checksum& 0x3F) + 0x20;

            if (checksum == buffteleinfo[bufflen-2]) { // Vérification du checksum
                traitbuf_cpt(buffteleinfo,bufflen-1); // ChekSum OK => Analyse de la Trame
            }
        }
    }
}


void enregistrer ()
{
    char *nomFichier = "hist.log";
    File teleinfoFile;

    if (!SD.exists(nomFichier)) {
        teleinfoFile = SD.open(nomFichier, FILE_WRITE);

        teleinfoFile.print(F("date "));

        if (num_abo == 1) { // abo_BASE
            teleinfoFile.print(F("BASE"));
        }
        else if (num_abo == 2) { // abo_HCHP
            teleinfoFile.print(F("HCHP HCHC"));
        }
        else if (num_abo == 3) { // abo_EJP
            teleinfoFile.print(F("EJPHN EJPHPM"));
        }
        else if (num_abo == 4) { // abo_BBR
            teleinfoFile.print(F("BBRHCJB BBRHPJB BBRHCJW BBRHPJW BBRHCJR BBRHPJR"));
        }

        if (type_mono_tri[0] == 1) {
            teleinfoFile.print(F(" IINST PAPP"));
        }
        if (type_mono_tri[0] == 3) {
            teleinfoFile.print(F(" I ph1,ph2,ph3,P VA"));
        }
        if (cpt2_present) {
            if (type_mono_tri[0] == 1) {
                teleinfoFile.println(F("| Base cpt 2,I A cpt2,P VA cpt2"));
            }
            if (type_mono_tri[0] == 3) {
                teleinfoFile.println(F("| Base cpt 2,I ph1,ph2,ph3,P VA cpt2"));
            }
        }
        else {
            teleinfoFile.println("");
        }
    }
    else {
        teleinfoFile = SD.open(nomFichier, FILE_WRITE);
    }

    if (teleinfoFile) {
        char date_heure[18];
        sprintf(date_heure, "%d-%02d-%02d %02d:%02d", annee, mois, jour, heure, minute);

        teleinfoFile.print(date_heure);
        teleinfoFile.print(" ");

        teleinfoFile.print(INDEX1);
        teleinfoFile.print(" ");
        if (num_abo > 1) { // abo_HCHP ou EJP
            teleinfoFile.print(INDEX2);
            teleinfoFile.print(" ");
        }
        if (num_abo > 3){ // abo_BBR
            teleinfoFile.print(INDEX3);
            teleinfoFile.print(" ");
            teleinfoFile.print(INDEX4);
            teleinfoFile.print(" ");
            teleinfoFile.print(INDEX5);
            teleinfoFile.print(" ");
            teleinfoFile.print(INDEX6);
            teleinfoFile.print(" ");
        }

        teleinfoFile.print(IINST[0][0]);
        teleinfoFile.print(" ");
        if (type_mono_tri[0] == 3){
            teleinfoFile.print(IINST[0][1]);
            teleinfoFile.print(" ");
            teleinfoFile.print(IINST[0][2]);
            teleinfoFile.print(" ");
        }
        teleinfoFile.print(papp);

        if (cpt2_present) {
            teleinfoFile.print(" ");
            teleinfoFile.print(cpt2index);
            teleinfoFile.print(" ");
            teleinfoFile.print(IINST[1][0]);
            teleinfoFile.print(" ");
            if (type_mono_tri[1] == 3){
                teleinfoFile.print(IINST[1][1]);
                teleinfoFile.print(" ");
                teleinfoFile.print(IINST[1][2]);
                teleinfoFile.print(" ");
            }
            teleinfoFile.print(cpt2puissance);
        }

        teleinfoFile.println("");
        teleinfoFile.flush();
        teleinfoFile.close();
    }
    else {
        serie.println(F("Erreur fichier"));
    }
}


// Convert normal decimal numbers to binary coded decimal
static uint8_t bin2bcd (uint8_t val) { return val + 6 * (val / 10); }
