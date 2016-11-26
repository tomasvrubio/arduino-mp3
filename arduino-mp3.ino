//Librerias utilizadas en el codigo.
#include "U8glib.h"
#include <AGMp3.h>
//#include <avr/pgmspace.h> De momento no me hace falta hasta que no vea si voy a utilizar PROGMEM.

//Definiciones de parametros 
#define MAX_LONG 32         //Maxima longitud de linea que puedo mostrar por pantalla con el tamaño de letra escogido.
#define MAX_LONG_FICH 18    //Tamaño maximo que se necesita para utilizar el nombre entero de un fichero mp3 (el que tiene la ruta de directorio mas larga).
#define MENU_LONG 5
#define MAX_VOL 0
#define MIN_VOL 100
#define INC_VOL 5
#define NO_KEY 0
#define EncoderChnA 5       //Efntrada digital encoder 1.
#define EncoderChnB 4       //Entrada digital encoder 2.
#define EncoderBttn 0       //Entrada analogica (Botones A, B y E).
#define DisplayPin 3        //Para utilizar con el transistor y apagar/encender el LCD (¿esta libre el pin 3?).
#define TIEMPO_MAX 30000    //Tiempo en milisegundos para que se apague la pantalla si no se toca ningun boton.
#define WIDTH  128          //Anchura de la linea del LCD. (Lo pongo a fuego para ahorrar calculos posteriores)         //POR HACER: Poner el numero que corresponde.
#define HEIGTH 7            //Altura de los caracteres utilizados. (Lo pongo a fuego para ahorrar calculos posteriores) //POR HACER: Poner el numero que corresponde.
#define RAIZ "LIST.TXT"


//Variables globales
//MP3
AGMp3Player MP3player;
byte vol;
byte progreso; 

//PANTALLA
U8GLIB_ST7920_128X64_1X u8g(0, 1, 10);  //Orden de variables definicion: SCK, MOSI, CS.

//Encoder
static const byte stat_seq[]={3,2,0,1,3,2,0,1,3}; //CAMBIAR
//const PROGMEM byte stat_seq[]={3,2,0,1,3,2,0,1,3};
byte stat_seq_ptr;

//Propio del menu/lectura SD
int menu_current = 0;
boolean menu_redraw_required = 0;
char info [3][MAX_LONG];
char nombreFichero [MAX_LONG_FICH];
byte nivel;
byte id_cancion[3];
int maxEntradas =  0;
int cancion_aleatorio = 0;
unsigned long tiempo_luz;
boolean luz;



//------------------------------------------------------------------------------
/* \brief getKey : Identifica si se ha pulsado algun boton o se ha girado el encoder.
 *
 * \param[out] Caracter que indica boton pulsado.
 *
 * \return Diferentes valores:
 *  - R : Girado encoder a la izquierda.
 *  - A : Girado encoder a la derecha.
 *  - B : Pulsado boton B1.
 *  - M : Pulsado boton B2.
 *  - P : Pulsado boton Benc.
 *  - NO_KEY : No se ha realizado ninguna accion.
 */
byte getKey()
{
  int valueBttn1;
  byte stat_int=(digitalRead(EncoderChnB)<<1) | digitalRead(EncoderChnA); //Comprobamos si se ha modificado el Encoder
  
  if (stat_int==stat_seq[stat_seq_ptr+1])
  {
    stat_seq_ptr++;
    if (stat_seq_ptr==8)
    {
      stat_seq_ptr=4;
      return 'R'; //Movimiento hacia la izquierda.
    }
  }
  else if (stat_int==stat_seq[stat_seq_ptr-1])
  {
    stat_seq_ptr--;
    if (stat_seq_ptr==0)
    {
      stat_seq_ptr=4;
      return 'A'; //Movimiento hacia la derecha.
    }
  }

  //Comprobamos si se han modificado los botones.
  valueBttn1 = analogRead(EncoderBttn);
  if (valueBttn1 > 150)
  {
    if (antireboteAnalogico(EncoderBttn))
    {  
      if (valueBttn1 > 200 && valueBttn1 < 220)       //Boton B1.
        return 'B';
      else if (valueBttn1 > 288 && valueBttn1 < 308)  //Boton B2.
        return 'M';
      else if (valueBttn1 > 667 && valueBttn1 < 687)  //Boton Benc.
        return 'P';
    }
  }

  return NO_KEY;
}


//------------------------------------------------------------------------------
/* \brief antireboteAnalogico : Evita deteccion de varias pulsaciones muy seguidas
 *  en una entrada analogica. Se mantiene dentro de la funcion hasta que consigue 
 *  que el primer valor leido sea diferente (se deje de pulsar).
 *
 * \param[in] pin : Boton a revisar si esta pulsado.
 * \param[out] Estado del boton (lectura analogica del pin).
 *
 * \return Lectura analogica en el pin de entrada.
 * 
 * \see Mirar la posibilidad de detectar pulsaciones largas.
 */
boolean antireboteAnalogico(int pin) {
  int contador=0;
  int estado;            // Guarda el estado del boton.
  int estadoAnterior;    // Guarda el ultimo estado del boton. 

  do {
    estado = analogRead(pin);
    if (estado!=estadoAnterior){  // Comparamos el estado actual. 
      contador = 0;               // Reiniciamos el contador.
      estadoAnterior = estado;
    }
    else
      contador = contador+1;      // Por cada vuelta sin cambiar aumenta en 1 el contador.
  } while (contador<5);   //Contador de tiempo antirrebote.
  
  return estado;
}


//------------------------------------------------------------------------------
/* \brief lecturaEntradas : Elige el nombre de fichero en funcion de las variables
 *  que le son pasadas. 
 *
 * \param[in] id_track[0-2] : Identificador de navegacion por directorio
 */
void preparaNombreFichero(byte artista, byte album, byte cancion)
{
  if (cancion != 0)
    sprintf(nombreFichero, "A%03d/D%03d/C%03d.MP3", artista, album, cancion);
  else{
    if (album != 0)
      sprintf(nombreFichero, "A%03d/D%03d/LIST.TXT", artista, album);
    else{
      if (artista != 0)
        sprintf(nombreFichero, "A%03d/LIST.TXT", artista);
      else
        sprintf(nombreFichero, RAIZ);
    }
  }
}


//------------------------------------------------------------------------------
/* \brief lecturaEntradas : En base al punto en el que nos encontramos y a la 
 *  pulsacion que ejecuta el usuario se realiza una acción u otra.
 *
 * \param[in] total_lineas : Total de lineas que tiene el nivel actual.
 *
 * \see Ver la manera de recortar la longitud del codigo juntando partes comunes
 *  de 'B' y 'P'. ¿Hacer una funcion que cree el nombre del fichero en funcion
 *  de las variables recibidas? Se podria reutilizar en buena parte del codigo.
 */
void lecturaEntradas(byte total_lineas) {
  char tempB=getKey();
  

  if (tempB!=NO_KEY){
    if (luz==0){ //Si la luz esta apagada al pulsar un boton lo unico que se hace es encender la luz.
      luz=1;
      digitalWrite(DisplayPin, 1);
      tiempo_luz = millis();        
    }
    else {
      
      switch (tempB) {
        case 'R': //En caso de girar a la izquierda la rueda.
          switch(nivel){
            case 1:
            case 2:
            case 3:
              if (menu_current>1)
                menu_current--;
              break;
            case 4:
              if (vol<MIN_VOL-INC_VOL){
                vol = vol + INC_VOL;
                MP3player.setVolumen(vol,vol);
              }
              break;
          }
          progreso=0;
          break;
        case 'A': //En caso de girar a la derecha la rueda.
          switch(nivel){
            case 1:
            case 2:
            case 3:
              if (menu_current<total_lineas)
                menu_current++;
              break;
            case 4:
              if (vol>MAX_VOL){
                vol = vol - INC_VOL;
                MP3player.setVolumen(vol,vol);
              }
              break;
          }
          progreso=0;
          break;
        case 'P': //En caso de pulsar el boton del encoder.
          switch(nivel){
            case 1:
            case 2:
            case 3:
              id_cancion[nivel-1] = menu_current;
              copiaInfo(id_cancion[nivel-1]-1, nivel-1, nombreFichero);
              preparaNombreFichero(id_cancion[0], id_cancion[1], id_cancion[2]);
              if (nivel==3){
                MP3player.tocaMP3(nombreFichero);
                progreso = 100;
              }
              else{
                menu_current = 1;
                maxEntradas = cuentaFichero(nombreFichero);
              }
              nivel++;
              break;
            case 4:
              break;
          }
          break;
        case 'B': //En caso de pulsar el boton verde izquierdo
          switch(nivel){
            case 1: //¿que funcionalidad le puedo poner?
              menu_current-=min(4,menu_current);
              break;
            case 2:
            case 3:
            case 4:
            if (nivel==4){
              MP3player.paraMp3();
              cancion_aleatorio=0;
            }
            menu_current = id_cancion[nivel-2];
            id_cancion[nivel-2] = 0;
            preparaNombreFichero(id_cancion[0], id_cancion[1], id_cancion[2]);
            maxEntradas = cuentaFichero(nombreFichero);
            memset(info[nivel-2],0,MAX_LONG); //Cogiendo directamente la longitud de la linea.
            nivel--;
            break;
          }
          break;
        case 'M': //En caso de pulsar el boton verde derecho
          switch(nivel){
            case 1:
            case 2:
              menu_current+=min(4,total_lineas-menu_current);
              break;
            case 3:
              maxEntradas = generaAleatorio();
              siguienteAleatorio();
              MP3player.tocaMP3(nombreFichero);
              menu_current = cancion_aleatorio;
              progreso = 100; 
              nivel=4;
              break;
            case 4:
              //Si pulsamos mientras estamos escuchando una cancion se pasa a la siguiente.
              MP3player.paraMp3();
              delay(100);
              if (cancion_aleatorio==0){
                siguienteCancion();
                menu_current = id_cancion[nivel-2]; 
              }
              else{
                siguienteAleatorio();
                menu_current++;  
              }
              MP3player.tocaMP3(nombreFichero);
              progreso = 100;               
              break;
          }
          break;     
      }
      menu_redraw_required = 1;
    }
    tiempo_luz = millis(); //Al haber pulsado un boton ponemos el tiempo de ultima actualizacion a el instante en que nos encontramos.
  }
}


//------------------------------------------------------------------------------
/* \brief cuentaFichero : Contabiliza el numero total de lineas que contiene el 
 *  fichero solicitado por el parametro de entrada.
 *
 * \param[in] nombre : Descripcion del fichero del que se desea el numero de lineas.
 * \param[out] Lineas fichero.
 *
 * \note Es necesario que los ficheros contengan una linea en blanco al final de los
 *  mismos para que la logica de la funcion funcione correctamente.
 * 
 * \return Lineas que contiene el fichero (no se devuelve la ultima linea en blanco).
 *  - 0 : Problemas a la hora de leer el fichero.
 *  - Resto : Lineas fichero.
 */
int cuentaFichero(const char* nombre) {

  File myFile = SD.open(nombre);
  unsigned long fLineas;
  
  if (myFile) {
    fLineas = myFile.size()/32;
  } 
  else {
    pintaMensaje("(C)FICH.");
    fLineas = 0;
  }

  myFile.close();
  return fLineas;
}


//------------------------------------------------------------------------------
/* \brief copiaInfo : Copia la linea del fichero indicada en el punto del array 
 *  "info" que corresponde.
 *
 * \param[in] pos_info : Numero de linea que se desea almacenar.
 * \param[in] campo : Indica la posicion que se utilizara para almacenar la informacion
 *  en "info".
 * \param[in] nombre : Fichero del que se desea recuperar la informacion.
 *
 * \note La variable de entrada campo puede contener 3 valores, donde el valor n
 *  correspondera con la posicion info[n-1]:
 *  1 : Artista.
 *  2 : Album.
 *  3 : Cancion / Informacion auxiliar para uso interno de funciones.
 *
 * \see Controlar fuera de la funcion que 0 significa que ha habido un error sacando la informacion del fichero.
 */
boolean copiaInfo(int pos_info, byte campo, const char* nombre) {

  File myFile = SD.open(nombre);
  byte pos_letra = 0;
  char letra;

  if (myFile) {
    myFile.seek(32*pos_info);
    while (myFile.available()) {
      letra = myFile.read();                
      if (letra!=10 && letra!=13){ 
        if (pos_letra<MAX_LONG-1){ //Solo me guardo las letras si no tengo llena ya la cadena donde almaceno la linea leida
          if (letra=='ñ')             
            letra = 'n'+125;          
          info[2][pos_letra]=letra;
          pos_letra++;
        }
      } else {
        info[2][pos_letra]=0;
        pos_letra=0;
        if (campo!=2){
          strncpy(info[campo],info[2],MAX_LONG);
          memset(info[2],0,MAX_LONG); //Borramos el contenido de info[2] ya que lo hemos utilizado de comodin (y solo guardamos su valor si venimos buscando info de la cancion)
        }
        myFile.close();
        return 1;       
      }
    }
  } else {
    pintaMensaje("(I)FICH.");
    myFile.close();
    return 0;
  }
}


//------------------------------------------------------------------------------
/* \brief dibujaMenu : Se muestra en el LCD las distintas opciones que se puede elegir 
 *  entre artistas, albumes o canciones en funcion del nivel en que nos encontramos (1, 
 *  2 o 3) en base al nombre de fichero que se pasa. Se marca con un recuadro la posicion 
 *  sobre la que nos encontramos del menu.
 *
 * \param[in] pos_actual : Posicion del listado en la que nos encontramos.
 * \param[in] total_lineas : Total de lineas que tiene el listado del menu.
 * \param[in] nombre : Fichero del que se extrae la informacion a mostrar.
 * 
 * \see Poner un control de error que devuelva un 0 si no ha sido capaz de leer el fichero.
 *  Mirar el resto de tipografias por si hay una mejor.
 *  ¿Podria hacer que los espacios ocuparan menos?
 */
void dibujaMenu(int pos_actual, int total_lineas, const char* nombre) {
  
  byte pos_inicio;
  byte pos_menu;
  byte pos_letra = 0;
  char letra;
  byte i = 0;
  File myFile = SD.open(nombre);
  
  //Calculo la posicion de inicio y la posicion en la que me encuentro en la matriz
  if (total_lineas<6) {
    pos_inicio = 0;
    pos_menu = pos_actual-1;
  }
  else {
    if (pos_actual<=3){
      pos_inicio = 0;
      pos_menu = pos_actual-1;
    }
    else if (pos_actual>3 && pos_actual<=total_lineas-2){
      pos_inicio = pos_actual-3;
      pos_menu = 2;
    }
    else{ 
      pos_inicio = total_lineas-MENU_LONG; 
      pos_menu = MENU_LONG-(total_lineas-pos_actual)-1;
    }
  }
  
  if (myFile) {
    //Mostramos la parte superior del menu
    u8g.drawStr(WIDTH/3 - 3, 0, F("ESCUCHA_ME"));
    u8g.drawHLine(0,8,WIDTH);
    u8g.drawHLine(0,23,WIDTH);

    while(i<nivel-1) {
      u8g.drawStr(i, (i+1)*HEIGTH +2, info[i]);
      i++;
    }

    i=0;
    myFile.seek(32*pos_inicio);
     
    while (myFile.available() && i<MENU_LONG) {
      letra = myFile.read();
      
      if (letra!=10 && letra!=13){ 
        if (pos_letra<MAX_LONG-1){ //Solo me guardo las letras si no tengo llena ya la cadena donde almaceno la linea leida
          if (letra=='ñ')             
            letra = 'n'+125;          
          info[2][pos_letra]=letra;
          pos_letra++;
        }
      } 
      else {
        info[2][pos_letra]=0;
        pos_letra=0;
         
        if (i==pos_menu) {
          u8g.drawBox(0, (i+4)*HEIGTH-3, WIDTH, HEIGTH);
          u8g.setDefaultBackgroundColor();
          u8g.drawStr(nivel-1, (i+4)*HEIGTH-3, info[2]);
          u8g.setDefaultForegroundColor();
        } 
        else
          u8g.drawStr(nivel-1, (i+4)*HEIGTH-3, info[2]);             
        i++;    
      }
    }
  }
  memset(info[2],0,MAX_LONG); //Borramos el contenido de info[2] ya que lo hemos utilizado de comodin
  myFile.close();
}


//------------------------------------------------------------------------------
/* \brief dibujaReproduc : Muestra en el LCD la información de la cancion que se esta
 *  reproduciendo. Solo se muestra cuando nos encontramos en el nivel 4.
 *
 * \param[in] cancion : Posicion de la cancion que se esta reproduciendo dentro del listado.
 * \param[in] total_canciones : Total de canciones en el listado que se esta reproduciendo.
 * \param[in] pos : Progreso completado de la cancion que se esta reproduciendo.
 * 
 * \see ¿Como puedo redistribuir la informacion en la pantalla?
 *  ¿Que otras tipografias puedo usar que sean interesantes?
 */
void dibujaReproduc(byte cancion, int total_canciones, int pos) {
  uint8_t i;
  char lineaCreada [MAX_LONG];

  u8g.drawStr(WIDTH/3 - 3, 0, F("ESCUCHA_ME"));
  u8g.drawHLine(0,8,WIDTH);

  //Mostramos los identificadores del fichero (Artista, Album, Cancion)
  for (i=0; i<nivel-1; i++)
    u8g.drawStr(i, (i)*HEIGTH+10, info[i]);
   
  //Mostramos barra progreso.
  u8g.drawFrame(14, (i)*HEIGTH+3+10, WIDTH-28, 5);
  u8g.drawBox(14, (i)*HEIGTH+4+10, pos, 3);

  //Mostramos volumen y numero de cancion.
  sprintf(lineaCreada, "VOL %02d/%02d", (MIN_VOL-vol)/INC_VOL, (MIN_VOL/INC_VOL));
  u8g.drawStr(1, (i+2)*HEIGTH+10, lineaCreada);
  sprintf(lineaCreada, "TRK %03d/%03d", cancion, total_canciones);
  u8g.drawStr(1, (i+3)*HEIGTH+10, lineaCreada);

  if (cancion_aleatorio!=0)
    u8g.drawStr(50, (i+3)*HEIGTH+10, F("ALEAT."));
}


//------------------------------------------------------------------------------
/* \brief pintaMensaje : Muestra en el LCD el mensaje que se ha pasado en la llamada
 *  a la funcion.
 *
 * \param[in] mensaje : Informacion a mostrar en la pantalla.
 */
void pintaMensaje(const char *mensaje) {
  u8g.firstPage();
  do  {
    u8g.drawStr(1, 24, mensaje);
  } while(u8g.nextPage());
}


//------------------------------------------------------------------------------
/* \brief siguienteCancion : Pasa a la siguiente cancion disponible.
 *  
 * \note Si es el final del album pasa al siguiente, si ya no quedan mas albumes
 *  del artista pasa al siguiente artista. Si ya no quedan mas artistas vuelve a
 *  la primera cancion.
 * 
 * \see Le puedo poner un return y devolver 0 si he llegado a la ultima cancion. Asi
 *  podria parar de reproducir una vez llegue a la ultima cancion.
 */
void siguienteCancion()
{
  preparaNombreFichero(id_cancion[0], 0, 0);

  if (id_cancion[2] < maxEntradas)                       
    id_cancion[2]++;
  else {
    id_cancion[2] = 1;
    if (id_cancion[1] < cuentaFichero(nombreFichero))     //Listado albumes
      id_cancion[1]++;
    else {
      id_cancion[1] = 1;
      if (id_cancion[0] < cuentaFichero(RAIZ)) //Listado artistas
        id_cancion[0]++;
      else  //Si hemos llegado al final volvemos a empezar
        id_cancion[0] = 1;
    } 
  }

  memset(info,0,sizeof(info));
  copiaInfo(id_cancion[0]-1, 0, RAIZ);
  copiaInfo(id_cancion[1]-1, 1, nombreFichero);
  preparaNombreFichero(id_cancion[0], id_cancion[1], 0);
  maxEntradas = cuentaFichero(nombreFichero);
  copiaInfo(id_cancion[2]-1, 2, nombreFichero);
    
  //Dejamos el nombre del mp3 preparado
  preparaNombreFichero(id_cancion[0], id_cancion[1], id_cancion[2]);

  return;
}


//------------------------------------------------------------------------------
/* \brief generaAleatorio : Genera la lista de canciones aleatorias que despues 
 *  reproduciremos. Cuenta el total de canciones que
 *  incluiria y genera un listado equivalente de numeros aleatorios. 
 *
 * \param[out] total_aleatorio: Numero de canciones incluidas en la lista de canciones
 *  aleatorias.
 *  
 * \note Deja en la SD dos ficheros generados "RAND.TXT" (con el listado de numeros 
 *  aleatorios) y "SONG.TXT" (listado de las canciones incluidas en el aleatorio).
 * 
 * \return Numero de canciones incluidas en el aleatorio:
 *  - 0 : Problemas a la hora de generar el aleatorio.
 *  - Resto : Numero de canciones.
 *  
 * \see Hacer algo despues de llamar a listadoAleatorio para tratar la devolucion de 0 numeros.
 */
int generaAleatorio()
{
  byte i=1;
  int numAleatorio=0, total_aleatorio;
  File myFile;
  char numero[4];

  pintaMensaje("Generando aleatorio...");

  //Si existe RAND_DIR, borrarlo (por si se ha quedado de una ejecucion erronea anterior)
  if (SD.exists(F("RAND"))){
    sprintf(nombreFichero, "RAND/%03d", i);
    while (SD.exists(nombreFichero)) {
      SD.remove(nombreFichero);
      i++;
      sprintf(nombreFichero, "RAND/%03d", i);
    }
    if (SD.exists(F("RAND/RAND.TXT")))
      SD.remove(F("RAND/RAND.TXT"));
  }
  else
    SD.mkdir(F("RAND"));

  //Inicializamos la semilla con un valor aleatorio sacado de una entrada sin utilizar para que no se repita la secuencia una y otra vez
  i=1; 
  randomSeed(analogRead(6));
  total_aleatorio = maxEntradas;

  //Generamos los numeros aleatorios
  while (i<=total_aleatorio){
    numAleatorio = random(1, total_aleatorio+1);
    sprintf(nombreFichero, "RAND/%03d", numAleatorio);
    myFile = SD.open(nombreFichero);
    
    if (myFile){
      myFile.close();
    }
    else {
      myFile = SD.open(nombreFichero, FILE_WRITE);
      myFile.close();
      myFile = SD.open(F("RAND/RAND.TXT"), FILE_WRITE);
      sprintf(numero, "%03d", numAleatorio);
      myFile.println(numero);
      myFile.close();
      i++;
    }
  }

  //Eliminamos los ficheros generados para controlar la generacion de numeros aleatorios
  for (i=1; i<=total_aleatorio; i++){
    sprintf(nombreFichero, "RAND/%03d", i);
    SD.remove(nombreFichero);
  }
  
  return total_aleatorio;
}


//------------------------------------------------------------------------------
/* \brief siguienteAleatorio : Pasa a la siguiente cancion disponible dentro del
 *  listado aleatorio.
 *  
 * \note Si llega al final del aleatorio vuelve a comenzar desde el principio.
 * 
 * \see Le puedo poner un return y devolver 0 si he llegado a la ultima cancion. O bien
 *  algun numero mostrando que he tenido un error (cual?). 
 */
void siguienteAleatorio()
{
  char cad_temporal[4];
  byte i;
  File myFile;
  
  if (cancion_aleatorio < maxEntradas)
    cancion_aleatorio++;
  else{
    //cancion_aleatorio = 1;  //De momento voy a dejar el aleatorio en repeat hasta que decida que hacer
    //menu_current = 0;
    id_cancion[2] = maxEntradas;//Me situo en la ultima cancion del album.
    siguienteCancion(); //Paso al siguiente album o artista al encontrarme en la ultima cancion.
    maxEntradas = generaAleatorio();
    menu_current = 0;
    cancion_aleatorio = 1;
  }

  myFile = SD.open(F("RAND/RAND.TXT"));
  if (myFile) {
    myFile.seek(5*(cancion_aleatorio-1));
    for (i=0;i<3;i++)
      cad_temporal[i]=myFile.read();
    cad_temporal[3]=0;
    myFile.close();    
  }
  else {
    pintaMensaje("(A)FICH.");
    myFile.close();
  }
  
  id_cancion[2] = atoi(&cad_temporal[0]);

  memset(info[2],0,MAX_LONG);
  preparaNombreFichero(id_cancion[0], id_cancion[1], 0); //Sacamos el nombre de la cancion
  copiaInfo(id_cancion[2]-1, 2, nombreFichero);
    
  //Dejamos el nombre del mp3 preparado
  preparaNombreFichero(id_cancion[0], id_cancion[1], id_cancion[2]);
}



void setup()
{
  luz=1;
  pinMode(DisplayPin, OUTPUT);
  digitalWrite(DisplayPin, 1);
  tiempo_luz = millis();
  
  pinMode(EncoderChnA, INPUT);
  digitalWrite(EncoderChnA, HIGH);
  pinMode(EncoderChnB, INPUT);
  digitalWrite(EncoderChnB, HIGH);
  stat_seq_ptr=4; // Center the status of the encoder
  
  //Inicializamos MP3 (dentro se inicializa la SD.
  if ( MP3player.inicializa() != 0 ) {
      pintaMensaje("NO SD.");
  }

  //Inicializacion variables
  menu_current = 1;
  id_cancion[0] = 0; //Los inicio en 0 ya que luego van tomando los valores con menu_current
  id_cancion[1] = 0;
  id_cancion[2] = 0;
  nivel = 1;
  preparaNombreFichero(0, 0, 0); //Ponemos el catalogo de artistas.
  maxEntradas = cuentaFichero(nombreFichero);
  menu_redraw_required = 1;
  vol=80;
  
  MP3player.setVolumen(vol, vol);
  progreso = 100;
  delay(100);

  //Configuracion del LCD.
  u8g.setRot180();
  u8g.setFont(u8g_font_babyr);
  u8g.setFontRefHeightText();
  u8g.setFontPosTop();
  u8g.setDefaultForegroundColor();
}


void loop(void) {
  //Cuando pasa el tiempo marcado se apaga la pantalla hasta que volvamos a tocar un boton.
  if (luz==1 && millis()>(tiempo_luz+TIEMPO_MAX)){
      digitalWrite(DisplayPin, 0);
      luz=0;
  }

  //Si se ha pedido actualizacion de la pantalla cambiamos la informacion que mostramos en el LCD.
  if (menu_redraw_required!=0) {
    if (nivel<4){ //Si estamos en nivel 1, 2 o 3 mostramos el menu
      u8g.firstPage();
      do  {
        dibujaMenu(menu_current, maxEntradas, nombreFichero);
      } while(u8g.nextPage());
      menu_redraw_required = 0;
    }
    else { //Al estar reproduciendo mostramos la informacion de la cancion por pantalla
      if (progreso!=MP3player.getPosicion()){ //Comprobar que ha cambiado el punto en el que se encontraba la cancion
        progreso = MP3player.getPosicion();
        u8g.firstPage();
        do  {
            dibujaReproduc(menu_current, maxEntradas, progreso);        
        } while(u8g.nextPage());  
      }    
    }
  }

  //Si termina de reproducir una cancion hay que pasar a la siguiente
  if (nivel==4){  
    if (!MP3player.estaTocando()){
      if (cancion_aleatorio==0){
        siguienteCancion();
        menu_current = id_cancion[2];
      }
      else{
        siguienteAleatorio();
        menu_current++;
      }
        
      MP3player.tocaMP3(nombreFichero);
      progreso = 100;
      menu_redraw_required = 1;
    }
  }

  //Leemos si hay alguna actualización en los botones/encoder
  lecturaEntradas(maxEntradas);
}
