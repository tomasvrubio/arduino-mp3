//Librerias utilizadas en el codigo.
#include <SPI.h>
#include <SD.h>
#include "U8glib.h"
#include <AGMp3.h>

//Definiciones de parametros 
#define MAX_LONG 21         //¿Cuanto es lo maximo que puedo mostrar en la pantalla? No merece la pena almacenar mas longitud, no??
#define MENU_LONG 5
#define MAX_VOL 0
#define MIN_VOL 100
#define INC_VOL 5
#define NO_KEY 0
#define EncoderChnA 5       //Entrada digital encoder 1.
#define EncoderChnB 4       //Entrada digital encoder 2.
#define EncoderBttn 0       //Entrada analogica (Botones A, B y E).
#define DisplayPin 3        //Para utilizar con el transistor y apagar/encender el LCD (¿esta libre el pin 3?).
#define TIEMPO_MAX 30000    //Tiempo en milisegundos para que se apague la pantalla si no se toca ningun boton.

//Variables globales
//MP3
AGMp3Player MP3player;
byte vol;
byte progActual;
byte progAntiguo;
//PANTALLA
U8GLIB_ST7920_128X64_1X u8g(0, 1, 10);  // Orden de variables definicion: SCK, MOSI, CS.
//Encoder
static const byte stat_seq[]={3,2,0,1,3,2,0,1,3};
byte stat_seq_ptr;
//Propio del menu/lectura SD
int menu_current = 0;
boolean menu_redraw_required = 0;
boolean reprod_redraw_required = 0;
char strLinea [MAX_LONG]; //¿Esta variable la necesito como global? ¿Creo que si?
char info [3][MAX_LONG];
char nombreFichero [31];
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
  
  //Comprobamos si se ha modificado el Encoder
  byte stat_int=(digitalRead(EncoderChnB)<<1) | digitalRead(EncoderChnA);
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
    else{
      contador = contador+1;      // Por cada vuelta sin cambiar aumenta en 1 el contador.
    }
  } while (contador<5);   //Contador de tiempo antirrebote.
  
  return estado;
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
          break;
        case 'P': //En caso de pulsar el boton del encoder.
          switch(nivel){
            case 1:
              id_cancion[nivel-1] = menu_current;
              menu_current = 1;
              copiaInfo(id_cancion[nivel-1], nivel-1, nombreFichero);
              sprintf(nombreFichero, "ARTIS%03d/ALBUM.TXT", id_cancion[nivel-1]);
              maxEntradas = cuentaFichero(nombreFichero);
              nivel++;
              break;
            case 2:
              id_cancion[nivel-1] = menu_current;
              menu_current = 1;
              copiaInfo(id_cancion[nivel-1], nivel-1, nombreFichero);
              sprintf(nombreFichero, "ARTIS%03d/ALBUM%03d/CANCION.TXT", id_cancion[nivel-2], id_cancion[nivel-1]);
              maxEntradas = cuentaFichero(nombreFichero);
              nivel++;            
              break;
            case 3:
              id_cancion[nivel-1] = menu_current;
              copiaInfo(id_cancion[nivel-1], nivel-1, nombreFichero);
              sprintf(nombreFichero, "ARTIS%03d/ALBUM%03d/CANCI%03d.MP3", id_cancion[nivel-3], id_cancion[nivel-2], id_cancion[nivel-1]);
              MP3player.tocaMP3(nombreFichero);
              //Hacer algo si no consigo reproducir la cancion
              nivel++;
              break;
            case 4:
              break;
          }
          break;
        case 'B': //En caso de pulsar el boton verde izquierdo
          switch(nivel){
            case 2:
              menu_current = id_cancion[nivel-2];
              sprintf(nombreFichero, "ARTISTA.TXT");
              maxEntradas = cuentaFichero(nombreFichero);
              memset(info[nivel-2],0,sizeof(info[nivel-1])); //REVISAR: Tendria que borrar el tamaño del mismo campo que borro, no?
              nivel--;
              break;
            case 3:
              menu_current = id_cancion[nivel-2];
              sprintf(nombreFichero, "ARTIS%03d/ALBUM.TXT", id_cancion[0]);
              maxEntradas = cuentaFichero(nombreFichero);
              memset(info[nivel-2],0,sizeof(info[nivel-1]));
              nivel--;            
              break;
            case 4:
              MP3player.paraMp3();
              menu_current = id_cancion[nivel-2];
              sprintf(nombreFichero, "ARTIS%03d/ALBUM%03d/CANCION.TXT", id_cancion[0], id_cancion[1]);        
              maxEntradas = cuentaFichero(nombreFichero);
              memset(info[nivel-2],0,sizeof(info[nivel-1]));
              cancion_aleatorio=0; //Desactivo el aleatorio (en caso de que no lo estuviese utilizando esta linea no afecta).
              nivel--;
              break;
          }
          break;
        case 'M': //En caso de pulsar el boton verde derecho
          switch(nivel){
            case 1:
            case 2:
            case 3:
              //En funcion del nivel en que nos encontremos el aleatorio es de todo, artista o album.
              pintaMensaje("Creando aleatorio...");
              maxEntradas = listadoAleatorio(nivel);
              siguienteAleatorio();
              MP3player.tocaMP3(nombreFichero);
              nivel=4;
              break;
            case 4:
              //Si pulsamos mientras estamos escuchando una cancion se pasa a la siguiente.
              MP3player.paraMp3();
              if (cancion_aleatorio==0)
                siguienteCancion();
              else
                siguienteAleatorio();
              MP3player.tocaMP3(nombreFichero);
              menu_current = id_cancion[nivel-2];
              reprod_redraw_required = 1;      
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
int cuentaFichero(char* nombre) {

  File myFile = SD.open(nombre);
  int lineas = 0;

  if (myFile) {
    while (myFile.available()) {   
      if (myFile.read()==10)
        lineas++;
    }
    myFile.close();
  } else {
    pintaMensaje("(C)PROBLEMA FICHERO.");
  }

  return lineas;
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
boolean copiaInfo(int pos_info, byte campo, char* nombre) {

  //Variables
  File myFile = SD.open(nombre);  
  byte pos_letra = 0;
  byte len = 0;
  char letra;
  int i = 0;
  uint8_t encontrado = 0;
  
  if (myFile) {
    while (myFile.available() && encontrado==0) { //Una vez saque la informacion termino de recorrer el fichero aunque no se haya acabado.
      letra = myFile.read();  
            
      if (i<pos_info-1){ //Hasta que no he llegado a la linea de interes entro aqui para avanzar a la siguiente linea.
        if (letra==10)
          i++;        
      } 
      else if (i==pos_info-1){ //Ya he llegado a la linea solicitada.

        if (letra!=10 && letra!=13){ 
          if (letra=='ñ')             
            letra = 'n'+125;          
          strLinea[pos_letra]=letra;
          pos_letra++;
        
        } else {
          strLinea[pos_letra]=0;
          pos_letra=0;
 
          len = min(strlen(strLinea),20); //Me quedo con la longitud de la cadena solo si mide menos de 20 caracteres (tamaño maximo del array).
          if ( len > 0 ) {
            strncpy(info[campo],strLinea,len);
            encontrado = 1;
          }          
        }
      }
    }
    myFile.close();
  } else {
    pintaMensaje("(I)PROBLEMA FICHERO.");
    return 0;
  }

  return 1;
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
void dibujaMenu(int pos_actual, int total_lineas, char* nombre) {
  uint8_t h;
  u8g_uint_t w, d;
  byte pos_inicio;
  byte pos_menu;
  byte pos_letra = 0;
  byte len = 0;
  char letra;
  byte i = 0;
  File myFile = SD.open(nombre);

  //MOSTRAMOS EL LISTADO DEL MENU
  //TIPOGRAFIA DEL MENU
  u8g.setFont(u8g_font_6x10r); 
  u8g.setFontRefHeightText();
  u8g.setFontPosTop();

  //Calculamos la altura 
  h = u8g.getFontAscent()-u8g.getFontDescent();

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
     //MOSTRAMOS LA PARTE SUPERIOR DEL MENU
    w = u8g.getWidth();
    u8g.setDefaultForegroundColor();
    u8g.drawHLine(0,17,w);

    //Me sirve para ver en que numeros me estoy moviendo y asi puedo corregir en un futuro los desplaces que tengo entre contadores de funciones
    //sprintf(info[0], "%03d %03d %03d %03d", pos_actual, total_lineas, pos_menu, pos_inicio);

    while(i<nivel-1) {
      u8g.drawStr(i, (i)*h, info[i]); 
      i++;
    }

    i=0;
    
    while (myFile.available()) {      
      letra = myFile.read();

      if (i<pos_inicio){ //REVISAR: Tratar de coger lineas enteras hasta que llegue a la linea a la que necesito llegar.
        if (letra==10)
          i++;
      } 
      else if (i>=pos_inicio && i<pos_inicio+MENU_LONG){

        if (letra!=10 && letra!=13){ 
          if (pos_letra<MAX_LONG-1){ //Solo me guardo las letras si no tengo llena ya la cadena donde almaceno la linea leida
            if (letra=='ñ')             
              letra = 'n'+125;          
            strLinea[pos_letra]=letra;
            pos_letra++;
          }
        } else {
          strLinea[pos_letra]=0;
          pos_letra=0;
 
          len = min(strlen(strLinea),20);
          if ( len > 0 ) {
            u8g.setDefaultForegroundColor();
            if (i==pos_menu+pos_inicio) {                           //saber en cual me encuentro. cambiarlo y ponerlo bien. sacar el valor fuera del bucle para no calcularlo varias veces
              u8g.drawBox(0, (i+2-pos_inicio)*h+1, w, h);
              u8g.setDefaultBackgroundColor();
            }          
            //sprintf(info[2], "%03d %03d %03d", i+2-pos_inicio, pos_actual, menu_current);
            //u8g.drawStr(nivel-1, (i+2-pos_inicio)*h, info[2]);
            u8g.drawStr(nivel-1, (i+2-pos_inicio)*h, strLinea);
            i++;
          }          
        }
      }
    }
    myFile.close();
  }
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
  uint8_t h, i;
  u8g_uint_t w, d;

  //MOSTRAMOS EL LISTADO DEL MENU
  //TIPOGRAFIA DEL MENU
  u8g.setFont(u8g_font_6x10r); 
  u8g.setFontRefHeightText();
  u8g.setFontPosTop(); 
  h = u8g.getFontAscent()-u8g.getFontDescent(); //Calculamos la altura
  w = u8g.getWidth();

  //MOSTRAMOS LA PARTE SUPERIOR DEL REPRODUCTOR
  for (i=0; i<nivel-1; i++)
    u8g.drawStr(i, (i)*h, info[i]);
   
  u8g.setDefaultForegroundColor();
  //u8g.drawHLine(0,(i)*h+1,w); //Calcular a que altura tengo que poner la linea

  //Muestro barra progreso
  u8g.drawFrame(14, (i)*h+2, w-28, 5);
  u8g.drawBox(14, (i)*h+3, pos, 3);

  //Muestro volumen
  sprintf(strLinea, " VOL.%02d/%02d TRK-%02d/%02d", (MIN_VOL-vol)/INC_VOL, (MIN_VOL/INC_VOL), cancion, total_canciones);
  u8g.drawStr(1, (i+1)*h, strLinea); 

  //Muestro cancion respecto a total
  /*if (cancion_aleatorio!=0){
    sprintf(strLinea, "  ALE %02d", cancion, total_canciones);  
    u8g.drawStr(1, (i+2)*h, strLinea); 
  }*/
}


//------------------------------------------------------------------------------
/* \brief pintaMensaje : Muestra en el LCD el mensaje que se ha pasado en la llamada
 *  a la funcion.
 *
 * \param[in] mensaje : Informacion a mostrar en la pantalla.
 */
void pintaMensaje(char *mensaje) {
  u8g.firstPage();
  do  {
    u8g.setDefaultForegroundColor();
    u8g.setFont(u8g_font_6x10r); 
    u8g.setFontRefHeightText();
    u8g.setFontPosTop();
  
    u8g.drawStr(1, 24, mensaje);
  } while(u8g.nextPage());   
}


//------------------------------------------------------------------------------
/* \brief listadoAleatorio : Genera la lista de canciones aleatorias que despues 
 *  reproduciremos. En funcion del nivel en el que hemos activado el aleatorio
 *  el listado incluye unas canciones u otras. Cuenta el total de canciones que
 *  incluiria y genera un listado equivalente de numeros aleatorios. 
 *
 * \param[in] caso : Informacion a mostrar en la pantalla.
 * \param[out] total_aleatorio: Numero de canciones incluidas en la lista de canciones
 *  aleatorias.
 *  
 * \note Deja en la SD dos ficheros generados "randlist.txt" (con el listado de numeros 
 *  aleatorios) y "songlist.txt" (listado de las canciones incluidas en el aleatorio).
 * 
 * \return Numero de canciones incluidas en el aleatorio:
 *  - 0 : Problemas a la hora de generar el aleatorio.
 *  - Resto : Numero de canciones.
 *  
 * \see Hacer algo despues de llamar a listadoAleatorio para tratar la devolucion de 0 numeros.
 */
int listadoAleatorio(byte caso)
{
  byte i,j,k;
  int total_aleatorio=0;
  int numAleatorio;
  char numero[6];
  char cadena_cancion[10];
  File myFile;
  byte inicio_i, final_i, inicio_j, final_j, final_k;

  //Si existe RAND_DIR, borrarlo (por si se ha quedado de una ejecucion erronea anterior)
  if (SD.exists("RAND_DIR")){
    for (i=1; i<=cuentaFichero("randlist.txt"); i++){
      sprintf(nombreFichero, "RAND_DIR/%05d", i);
      SD.remove(nombreFichero);
    }
    SD.rmdir("RAND_DIR");
  }
  //Eliminamos el ultimo fichero aleatorio que generamos
  if (SD.exists("songlist.txt"))
    SD.remove("songlist.txt");
  if (SD.exists("randlist.txt"))
    SD.remove("randlist.txt");


  //Calculamos el numero total de canciones y a la vez creamos una lista con las canciones que tienen que sonar
  switch(caso){
    case 1:
      inicio_i = 1; 
      final_i =  cuentaFichero("ARTISTA.TXT");
      inicio_j = 1;   
      break;
    case 2:
      inicio_i = id_cancion[0]; 
      final_i =  id_cancion[0];
      inicio_j = 1;
      sprintf(nombreFichero, "ARTIS%03d/ALBUM.TXT", id_cancion[0]);
      final_j =  cuentaFichero(nombreFichero);
      break;
    case 3:
      inicio_i = id_cancion[0]; 
      final_i =  id_cancion[0];
      inicio_j = id_cancion[1]; 
      final_j =  id_cancion[1];
      break;
  }

  for (i=inicio_i; i<=final_i; i++){
    //Para cada i tengo que calcular la cantidad de albumes que tiene ese artista (solo si caso=1)
    if (inicio_i!=final_i){
      sprintf(nombreFichero, "ARTIS%03d/ALBUM.TXT", i);      
      final_j = cuentaFichero(nombreFichero);
    }
    for (j=inicio_j; j<=final_j; j++){
      sprintf(nombreFichero, "ARTIS%03d/ALBUM%03d/CANCION.TXT", i, j);
      final_k = cuentaFichero(nombreFichero);
      sprintf(nombreFichero, "songlist.txt");
      myFile = SD.open(nombreFichero, FILE_WRITE);
      for (k=1; k<=final_k; k++){
        //Guardamos en el fichero de canciones cada una de las canciones que formaran parte de la lista aleatoria
        sprintf(cadena_cancion, "%03d%03d%03d", i, j, k);
        myFile.println(cadena_cancion);
        //Por cada pasada aumentamos en 1 el indicador del tamaño de la lista de numeros a generar
        total_aleatorio = total_aleatorio + 1;
      }
      myFile.close();
    }
  }

  //Creamos la carpeta donde van los ficheros que nos permitiran saber si el numero esta ya generado o no
  SD.mkdir("RAND_DIR");

  //Inicializamos la semilla con un valor aleatorio sacado de una entrada sin utilizar para que no se repita la secuencia una y otra vez
  i=1; 
  randomSeed(analogRead(6));

  //Generamos los numeros aleatorios
  while (i<=total_aleatorio){
    numAleatorio = random(1, total_aleatorio+1);
    sprintf(nombreFichero, "RAND_DIR/%05d", numAleatorio);
    myFile = SD.open(nombreFichero);
    
    if (myFile){
      myFile.close();
    }
    else {
      myFile = SD.open(nombreFichero, FILE_WRITE);
      myFile.close();
      myFile = SD.open("randlist.txt", FILE_WRITE);
      sprintf(numero, "%05d", numAleatorio);
      myFile.println(numero);
      myFile.close();
      i++;
    }
  }

  //Eliminamos los ficheros generados para controlar la generacion de numeros aleatorios
  for (i=1; i<=total_aleatorio; i++){
    sprintf(nombreFichero, "RAND_DIR/%05d", i);
    SD.remove(nombreFichero);
  }
  SD.rmdir("RAND_DIR");

  return total_aleatorio;
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
  sprintf(nombreFichero, "ARTIS%03d/ALBUM.TXT", id_cancion[0]);

  if (id_cancion[2] < maxEntradas)                       
    id_cancion[2]++;
  else {
    id_cancion[2] = 1;
    if (id_cancion[1] < cuentaFichero(nombreFichero))     //Listado albumes
      id_cancion[1]++;
    else {
      id_cancion[1] = 1;
      if (id_cancion[0] < cuentaFichero("ARTISTA.TXT")) //Listado artistas
        id_cancion[0]++;
      else  //Si hemos llegado al final volvemos a empezar
        id_cancion[0]=1;
    } 
  }

  memset(info,0,sizeof(info));
  copiaInfo(id_cancion[0], 0, "ARTISTA.TXT");
  copiaInfo(id_cancion[1], 1, nombreFichero);
  sprintf(nombreFichero, "ARTIS%03d/ALBUM%03d/CANCION.TXT", id_cancion[0], id_cancion[1]);
  maxEntradas = cuentaFichero(nombreFichero);
  copiaInfo(id_cancion[2], 2, nombreFichero);
    
  sprintf(nombreFichero, "ARTIS%03d/ALBUM%03d/CANCI%03d.MP3", id_cancion[0], id_cancion[1], id_cancion[2]);

  return;
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
  int linea_aleatorio;
  char cad_temporal[6];
  byte i, j;
  
  if (cancion_aleatorio < maxEntradas)
    cancion_aleatorio++;
  else
    cancion_aleatorio = 1;  //De momento voy a dejar el aleatorio en repeat hasta que decida que hacer

  copiaInfo(cancion_aleatorio, 2, "RANDLIST.TXT");

  for (i=0; i<5; i++)
    cad_temporal[i] = info[2][i];
  cad_temporal[5] = 0;
  linea_aleatorio = atoi(&cad_temporal[0]);

  copiaInfo(linea_aleatorio, 2, "SONGLIST.TXT");
  
  //Ahora tengo la informacion sobre la cancion que tengo que reproducir en info[2]
  cad_temporal[0] = '0';  //Para los tres identificadores estos caracteres tienen estos valores fijos ya que son de 3 digitos
  cad_temporal[1] = '0';
  cad_temporal[5] = 0;
  for (j=0; j<=2; j++){
    for (i=0; i<=2; i++)
      cad_temporal[i+2] = info[2][j*3+i];      
    
    id_cancion[j] = atoi(&cad_temporal[0]);
  }

  memset(info,0,sizeof(info));
  copiaInfo(id_cancion[0], 0, "ARTISTA.TXT");
  sprintf(nombreFichero, "ARTIS%03d/ALBUM.TXT", id_cancion[0]);
  copiaInfo(id_cancion[1], 1, nombreFichero);
  sprintf(nombreFichero, "ARTIS%03d/ALBUM%03d/CANCION.TXT", id_cancion[0], id_cancion[1]);
  copiaInfo(id_cancion[2], 2, nombreFichero);
  
  sprintf(nombreFichero, "ARTIS%03d/ALBUM%03d/CANCI%03d.MP3", id_cancion[0], id_cancion[1], id_cancion[2]);
  maxEntradas = cuentaFichero("RANDLIST.TXT");
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
      pintaMensaje("SD NO INSERTADA");
  }

  //Inicializacion variables
  menu_current = 1;
  id_cancion[0] = 0; //Los inicio en 0 ya que luego van tomando los valores con menu_current
  id_cancion[1] = 0;
  id_cancion[2] = 0;
  nivel = 1; 
  sprintf(nombreFichero, "ARTISTA.TXT");
  maxEntradas = cuentaFichero(nombreFichero);
  menu_redraw_required = 1;
  vol=80;

  MP3player.setVolumen(vol, vol);
  progAntiguo = 100;

  delay(100);
}


void loop(void) {

  //¿Puedo comprobar de alguna manera que se ha sacado la SD?
  /*while (){
    pintaMensaje("SD NO INSERTADA");
  }*/

  if (luz==1 && millis()>(tiempo_luz+TIEMPO_MAX)){ //Cuando pasa el tiempo marcado se apaga la pantalla hasta que volvamos a tocar un boton.
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
      progActual = MP3player.getPosicion();
      if (progActual!=progAntiguo || reprod_redraw_required==1){ //Comprobar que ha cambiado el punto en el que se encontraba la cancion 
        u8g.firstPage();
        do  {
          if (cancion_aleatorio==0)
            dibujaReproduc(menu_current, maxEntradas, progActual);
          else
            dibujaReproduc(cancion_aleatorio, maxEntradas, progActual);          
        } while(u8g.nextPage());  
      }
      progAntiguo = progActual;
      reprod_redraw_required = 0;      
    }
  }

  //Si termina de reproducir una cancion hay que pasar a la siguiente
  if (nivel==4){  
    if (!MP3player.estaTocando()){
      if (cancion_aleatorio==0)
        siguienteCancion();
      else
        siguienteAleatorio();
        
      MP3player.tocaMP3(nombreFichero);
      menu_current = id_cancion[2];
      menu_redraw_required = 1;
    }
  }

  //Leemos si hay alguna actualización en los botones/encoder
  lecturaEntradas(maxEntradas);
}
