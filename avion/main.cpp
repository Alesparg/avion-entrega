#include <GL/freeglut.h>
#include <cmath>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>


float toRadians(float degrees);

// Variables globales para la posición y rotación del avión
float posX = 0.0f;
float posY = 0.0f;
float rotacion = 0.0f; // Rotación del avión en grados

// Estado de los faros
bool farosEncendidos = false;

// Para control de teclas simultáneas
bool keys[256] = { false };
bool specialKeys[256] = { false };

// Para controlar el tiempo y movimiento
float deltaTime = 0.0f;
int lastFrameTime = 0;

// --- Textura de fondo ---
GLuint texturaFondo = 0;
int texturaFondoAncho = 0;
int texturaFondoAlto = 0;
bool texturaFondoCargada = false;

// Parámetros de cámara/proyección 
const float CAMARA_POS_Z = 2.0f;       
const float FOV_Y_DEGREES = 60.0f;     
const float AVION_MARGEN_X = 0.6f;     
const float AVION_MARGEN_Y = 0.5f;     
const float ENEMIGO_MARGEN_XY = 0.35f; 


void obtenerExtensionesVisibles(float& halfX, float& halfY) {
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    if (h <= 0) h = 1;
    float aspect = static_cast<float>(w) / static_cast<float>(h);
    halfY = static_cast<float>(std::tan(static_cast<double>(toRadians(FOV_Y_DEGREES * 0.5f))) * static_cast<double>(CAMARA_POS_Z));
    halfX = aspect * halfY;
}


inline void limitarDentroPantalla(float& x, float& y, float margenX, float margenY) {
    float halfX = 0.0f, halfY = 0.0f;
    obtenerExtensionesVisibles(halfX, halfY);
    float limiteX = (halfX * 0.98f) - margenX; 
    float limiteY = (halfY * 0.98f) - margenY;
    if (limiteX < 0.0f) limiteX = 0.0f; 
    if (limiteY < 0.0f) limiteY = 0.0f;

    if (x > limiteX) x = limiteX;
    if (x < -limiteX) x = -limiteX;
    if (y > limiteY) y = limiteY;
    if (y < -limiteY) y = -limiteY;
}


bool cargarBMP24(const char* ruta, std::vector<unsigned char>& datosRGB, int& ancho, int& alto) {
	std::ifstream archivo(ruta, std::ios::binary);
	if (!archivo.is_open()) return false;

	unsigned char cabecera[54];
	archivo.read(reinterpret_cast<char*>(cabecera), 54);
	if (archivo.gcount() != 54) return false;
	if (cabecera[0] != 'B' || cabecera[1] != 'M') return false;

	unsigned int offsetDatos = *reinterpret_cast<unsigned int*>(&cabecera[10]);
	unsigned int tamanoCabecera = *reinterpret_cast<unsigned int*>(&cabecera[14]);
	int anchoImg = *reinterpret_cast<int*>(&cabecera[18]);
	int altoImg = *reinterpret_cast<int*>(&cabecera[22]);
	unsigned short bitsPorPixel = *reinterpret_cast<unsigned short*>(&cabecera[28]);
	unsigned int compresion = *reinterpret_cast<unsigned int*>(&cabecera[30]);

	if (tamanoCabecera < 40) return false;
	if (bitsPorPixel != 24) return false; 
	if (compresion != 0) return false; 

	bool topDown = false;
	if (altoImg < 0) {
		altoImg = -altoImg;
		topDown = true;
	}

	
	archivo.seekg(offsetDatos, std::ios::beg);

	
	int bytesPorFilaArchivo = ((anchoImg * 3 + 3) & ~3);
	std::vector<unsigned char> bufferBGR(bytesPorFilaArchivo * altoImg);
	archivo.read(reinterpret_cast<char*>(bufferBGR.data()), bufferBGR.size());
	if (archivo.gcount() != static_cast<std::streamsize>(bufferBGR.size())) return false;

	
	datosRGB.resize(anchoImg * altoImg * 3);
	for (int y = 0; y < altoImg; ++y) {
		int filaOrigen = topDown ? y : (altoImg - 1 - y);
		const unsigned char* ptrFila = &bufferBGR[filaOrigen * bytesPorFilaArchivo];
		unsigned char* ptrDestino = &datosRGB[y * anchoImg * 3];
		for (int x = 0; x < anchoImg; ++x) {
			unsigned char b = ptrFila[x * 3 + 0];
			unsigned char g = ptrFila[x * 3 + 1];
			unsigned char r = ptrFila[x * 3 + 2];
			ptrDestino[x * 3 + 0] = r;
			ptrDestino[x * 3 + 1] = g;
			ptrDestino[x * 3 + 2] = b;
		}
	}

	ancho = anchoImg;
	alto = altoImg;
	return true;
}

void cargarTexturaFondo(const char* ruta) {
	std::vector<unsigned char> rgb;
	int w = 0, h = 0;
	if (!cargarBMP24(ruta, rgb, w, h)) {
		texturaFondoCargada = false;
		return;
	}

	if (texturaFondo == 0) {
		glGenTextures(1, &texturaFondo);
	}
	glBindTexture(GL_TEXTURE_2D, texturaFondo);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
	glBindTexture(GL_TEXTURE_2D, 0);

	texturaFondoAncho = w;
	texturaFondoAlto = h;
	texturaFondoCargada = true;
}

// --- Enemigo ---
struct Enemigo {
    float x, y, z;
    int energiaRestante;  
    bool activo;
    float velocidadX, velocidadY;
    int tiempoUltimoMovimiento;
};

Enemigo enemigo = { 1.5f, 1.0f, 0.0f, 100, true, 0.2f, 0.15f, 0 };

// --- Balas ---
struct Bala {
    float x, y, z;
    float dirX, dirY; 
    bool activa;
};
const int MAX_BALAS = 50;
Bala balas[MAX_BALAS];

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Función para convertir grados a radianes
float toRadians(float degrees) {
    return degrees * M_PI / 180.0f;
}

// Función para obtener la dirección cardinal
std::string getDireccionCardinal(float angulo) {

    while (angulo < 0) angulo += 360.0f;
    while (angulo >= 360.0f) angulo -= 360.0f;

    // Determinar dirección cardinal
    if (angulo >= 337.5f || angulo < 22.5f) return "Norte";
    if (angulo >= 22.5f && angulo < 67.5f) return "Noreste";
    if (angulo >= 67.5f && angulo < 112.5f) return "Este";
    if (angulo >= 112.5f && angulo < 157.5f) return "Sureste";
    if (angulo >= 157.5f && angulo < 202.5f) return "Sur";
    if (angulo >= 202.5f && angulo < 247.5f) return "Suroeste";
    if (angulo >= 247.5f && angulo < 292.5f) return "Oeste";
    return "Noroeste";
}

void inicializarBalas() {
    for (int i = 0; i < MAX_BALAS; ++i) {
        balas[i].activa = false;
    }
}

// Función para detectar colisión entre bala y enemigo
bool colisionBalaEnemigo(const Bala& bala, const Enemigo& enemigo) {
    if (!enemigo.activo) return false;
    float dx = bala.x - enemigo.x;
    float dy = bala.y - enemigo.y;
    float distancia = sqrt(dx * dx + dy * dy);
    return distancia < 0.25f; 
}

// Dispara balas desde las ametralladoras en las alas
void disparar() {
    
    float offsetX = 0.25f; 
    float offsetY = 0.19f; 
    float z = 0.0f;

    // Calcula la dirección de las balas basada en la rotación del avión
    float dirX = cos(toRadians(rotacion));
    float dirY = sin(toRadians(rotacion));

    // Transforma las posiciones de las ametralladoras según la rotación
    float radRotation = toRadians(rotacion);
    float cosRot = cos(radRotation);
    float sinRot = sin(radRotation);

    float leftGunX = posX + (offsetX * cosRot - (-offsetY) * sinRot);
    float leftGunY = posY + (offsetX * sinRot + (-offsetY) * cosRot);

    float rightGunX = posX + (offsetX * cosRot - offsetY * sinRot);
    float rightGunY = posY + (offsetX * sinRot + offsetY * cosRot);

    // Busca dos espacios libres en el arreglo
    for (int i = 0; i < MAX_BALAS; ++i) {
        if (!balas[i].activa) {
            // Bala ala izquierda
            balas[i].x = leftGunX;
            balas[i].y = leftGunY;
            balas[i].z = z;
            balas[i].dirX = dirX;
            balas[i].dirY = dirY;
            balas[i].activa = true;


            for (int j = i + 1; j < MAX_BALAS; ++j) {
                if (!balas[j].activa) {
                    balas[j].x = rightGunX;
                    balas[j].y = rightGunY;
                    balas[j].z = z;
                    balas[j].dirX = dirX;
                    balas[j].dirY = dirY;
                    balas[j].activa = true;
                    return;
                }
            }
            return;
        }
    }
}

// Mover balas según su dirección y verificar colisiones
void moverBalas(float deltaTime) {
    float velocidad = 2.0f; 
    float distancia = velocidad * deltaTime;

    for (int i = 0; i < MAX_BALAS; ++i) {
        if (balas[i].activa) {
            balas[i].x += balas[i].dirX * distancia;
            balas[i].y += balas[i].dirY * distancia;

            // Verificar colisión con enemigo
            if (enemigo.activo && colisionBalaEnemigo(balas[i], enemigo)) {
                enemigo.energiaRestante -= 10; 
                balas[i].activa = false; // Destruye la bala

                if (enemigo.energiaRestante <= 0) {
                    enemigo.energiaRestante = 0; // Asegurar que no sea negativo
                    enemigo.activo = false; // El enemigo es destruido
                }
            }

            // Si la bala está fuera de los límites visibles, desactivarla
            float halfX = 0.0f, halfY = 0.0f;
            obtenerExtensionesVisibles(halfX, halfY);
            float limiteX = halfX * 1.2f; // margen extra para que no desaparezca justo al borde
            float limiteY = halfY * 1.2f;
            if (balas[i].x > limiteX || balas[i].x < -limiteX ||
                balas[i].y > limiteY || balas[i].y < -limiteY) {
                balas[i].activa = false;
            }
        }
    }
}

// Dibuja balas
void dibujarBalas() {
    glColor3f(1.0f, 1.0f, 0.0f);
    for (int i = 0; i < MAX_BALAS; ++i) {
        if (balas[i].activa) {
            glPushMatrix();
            glTranslatef(balas[i].x, balas[i].y, balas[i].z);
            glutSolidSphere(0.025, 8, 8);
            glPopMatrix();
        }
    }
}

void dibujarEnemigo() {
    if (!enemigo.activo) return;

    GLUquadric* quad = gluNewQuadric();
    glPushMatrix();
    glTranslatef(enemigo.x, enemigo.y, enemigo.z);

    // Cuerpo del enemigo (rojo)
    glColor3f(1.0f, 0.0f, 0.0f);
    glutSolidSphere(0.2f, 16, 16);

    // Alas del enemigo
    glColor3f(0.8f, 0.0f, 0.0f);
    glPushMatrix();
    glScalef(0.4f, 0.1f, 0.05f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Indicador de energía (barra encima del enemigo)
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.3f);

    // Barra de fondo (roja)
    glColor3f(0.8f, 0.2f, 0.2f);
    glPushMatrix();
    glScalef(0.4f, 0.05f, 0.01f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Barra de energía (verde)
    float porcentajeEnergia = enemigo.energiaRestante / 100.0f;
    glColor3f(0.0f, 1.0f, 0.0f);
    glPushMatrix();
    glTranslatef(-(0.4f * (1.0f - porcentajeEnergia)) / 2.0f, 0.0f, 0.01f);
    glScalef(0.4f * porcentajeEnergia, 0.05f, 0.01f);
    glutSolidCube(1.0);
    glPopMatrix();

    glPopMatrix();
    glPopMatrix();

    gluDeleteQuadric(quad);
}

void actualizarEnemigo(float deltaTime) {
    static int tiempoDestruccion = 0;
    int tiempoActual = glutGet(GLUT_ELAPSED_TIME);

    if (!enemigo.activo) {
        // Regenerar enemigo si fue destruido (después de 3 segundos)
        if (tiempoDestruccion == 0) {
            tiempoDestruccion = tiempoActual;
        }
        else if (tiempoActual - tiempoDestruccion > 3000) {
            // Asegurar que el enemigo aparezca SIEMPRE dentro de los límites visibles
            enemigo.x = ((rand() % 500) - 250) / 100.0f; 
            enemigo.y = ((rand() % 500) - 250) / 100.0f; 
            enemigo.energiaRestante = 100;
            enemigo.activo = true;
            enemigo.velocidadX = ((rand() % 60) - 30) / 100.0f; 
            enemigo.velocidadY = ((rand() % 60) - 30) / 100.0f; 
            tiempoDestruccion = 0;
        }
        return;
    }

    // Cambiar dirección cada 3 segundos
    if (tiempoActual - enemigo.tiempoUltimoMovimiento > 3000) {
        enemigo.velocidadX = ((rand() % 60) - 30) / 100.0f; 
        enemigo.velocidadY = ((rand() % 60) - 30) / 100.0f; 
        enemigo.tiempoUltimoMovimiento = tiempoActual;
    }

    // Mover enemigo
    enemigo.x += enemigo.velocidadX * deltaTime;
    enemigo.y += enemigo.velocidadY * deltaTime;

    
    float halfX = 0.0f, halfY = 0.0f;
    obtenerExtensionesVisibles(halfX, halfY);
    float limiteX = (halfX * 0.98f) - ENEMIGO_MARGEN_XY;
    float limiteY = (halfY * 0.98f) - ENEMIGO_MARGEN_XY;
    if (limiteX < 0.0f) limiteX = 0.0f;
    if (limiteY < 0.0f) limiteY = 0.0f;

    if (enemigo.x > limiteX) {
        enemigo.x = limiteX;
        enemigo.velocidadX = -abs(enemigo.velocidadX);
    }
    if (enemigo.x < -limiteX) {
        enemigo.x = -limiteX;
        enemigo.velocidadX = abs(enemigo.velocidadX);
    }
    if (enemigo.y > limiteY) {
        enemigo.y = limiteY;
        enemigo.velocidadY = -abs(enemigo.velocidadY);
    }
    if (enemigo.y < -limiteY) {
        enemigo.y = -limiteY;
        enemigo.velocidadY = abs(enemigo.velocidadY);
    }
}

void dibujarFondoImagen() {
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(-5, 5, -5, 5);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    if (texturaFondoCargada) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texturaFondo);
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-5.0f,  5.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 5.0f,  5.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 5.0f, -5.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-5.0f, -5.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    } else {
        
        glBegin(GL_QUADS);
        glColor3f(0.0f, 0.3f, 0.7f);
        glVertex2f(-5.0f, 5.0f);
        glVertex2f(5.0f, 5.0f);
        glColor3f(0.4f, 0.6f, 1.0f);
        glVertex2f(5.0f, -5.0f);
        glVertex2f(-5.0f, -5.0f);
        glEnd();
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

void dibujarFaros() {
    
    float offsetX = 0.25f; 
    float offsetY = 0.19f; 
    float z = 0.025f; 

    // Transforma las posiciones de los faros según la rotación
    float radRotation = toRadians(rotacion);
    float cosRot = cos(radRotation);
    float sinRot = sin(radRotation);

    float leftLightX = posX + (offsetX * cosRot - (-offsetY) * sinRot);
    float leftLightY = posY + (offsetX * sinRot + (-offsetY) * cosRot);

    float rightLightX = posX + (offsetX * cosRot - offsetY * sinRot);
    float rightLightY = posY + (offsetX * sinRot + offsetY * cosRot);

    // Color de los faros
    glColor3f(1.0f, 1.0f, 1.0f);

    // Izquierdo
    glPushMatrix();
    glTranslatef(leftLightX, leftLightY, z);
    glutSolidSphere(0.015, 12, 12); 

    // Haz de luz si está encendido
    if (farosEncendidos) {
        glColor4f(1.0f, 1.0f, 0.7f, 0.3f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPushMatrix();
        glRotatef(rotacion, 0, 0, 1);
        glBegin(GL_TRIANGLES);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glVertex3f(0.125f, -0.025f, -0.005f); 
        glVertex3f(0.125f, 0.025f, 0.005f); 
        glEnd();
        glPopMatrix();
        glDisable(GL_BLEND);
    }
    glPopMatrix();

    // Derecho
    glColor3f(1.0f, 1.0f, 1.0f);
    glPushMatrix();
    glTranslatef(rightLightX, rightLightY, z);
    glutSolidSphere(0.015, 12, 12); 

    if (farosEncendidos) {
        glColor4f(1.0f, 1.0f, 0.7f, 0.3f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPushMatrix();
        glRotatef(rotacion, 0, 0, 1);
        glBegin(GL_TRIANGLES);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glVertex3f(0.125f, 0.025f, -0.005f); 
        glVertex3f(0.125f, -0.025f, 0.005f); 
        glEnd();
        glPopMatrix();
        glDisable(GL_BLEND);
    }
    glPopMatrix();
}

void dibujarAvion3D() {
    GLUquadric* quad = gluNewQuadric();
    glPushMatrix();
    // Aplica la posición y rotación
    glTranslatef(posX, posY, 0.0f);
    glRotatef(rotacion, 0.0f, 0.0f, 1.0f);

    
    glScalef(0.5f, 0.5f, 0.5f);

    // Fuselaje (cilindro largo)
    glColor3f(0.2f, 0.2f, 1.0f); // Azul
    glPushMatrix();
    glRotatef(90, 0, 1, 0);
    gluCylinder(quad, 0.07, 0.07, 1.0, 32, 32);
    // Punta delantera (cono)
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 1.0f);
    gluCylinder(quad, 0.07, 0.0, 0.15, 32, 1);
    glPopMatrix();
    // Tapa trasera (disco)
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.0f);
    gluDisk(quad, 0.0, 0.07, 32, 1);
    glPopMatrix();
    glPopMatrix();

    // Cabina (esfera pequeña en la parte superior delantera)
    glColor3f(0.8f, 0.8f, 1.0f); // Celeste
    glPushMatrix();
    glTranslatef(0.95f, 0.0f, 0.07f);
    glutSolidSphere(0.09, 24, 24);
    glPopMatrix();

    // Alas principales (trapezoidales)
    glColor3f(1.0f, 0.0f, 0.0f); // Rojo
    glPushMatrix();
    glTranslatef(0.2f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex3f(-0.1f, -0.5f, 0.0f);
    glVertex3f(0.5f, -0.25f, 0.0f);
    glVertex3f(0.5f, 0.25f, 0.0f);
    glVertex3f(-0.1f, 0.5f, 0.0f);
    glEnd();
    glPopMatrix();

    // Motores bajo las alas
    glColor3f(0.3f, 0.3f, 0.3f);
    // Motor izquierdo
    glPushMatrix();
    glTranslatef(0.35f, -0.28f, -0.07f);
    glRotatef(-90, 1, 0, 0);
    glutSolidCylinder(0.04, 0.15, 16, 16);
    glPopMatrix();
    // Motor derecho
    glPushMatrix();
    glTranslatef(0.35f, 0.28f, -0.07f);
    glRotatef(-90, 1, 0, 0);
    glutSolidCylinder(0.04, 0.15, 16, 16);
    glPopMatrix();

    // Cola vertical (aleta)
    glColor3f(0.0f, 1.0f, 0.0f); // Verde
    glPushMatrix();
    glTranslatef(-0.08f, 0.0f, 0.18f);
    glScalef(0.12f, 0.01f, 0.18f);
    glutSolidCube(1.0);
    glPopMatrix();
    // Cola horizontal izquierda
    glColor3f(0.0f, 1.0f, 0.0f); // Verde
    glPushMatrix();
    glTranslatef(-0.12f, -0.08f, 0.08f);
    glScalef(0.08f, 0.01f, 0.10f);
    glutSolidCube(1.0);
    glPopMatrix();
    // Cola horizontal derecha
    glPushMatrix();
    glTranslatef(-0.12f, 0.08f, 0.08f);
    glScalef(0.08f, 0.01f, 0.10f);
    glutSolidCube(1.0);
    glPopMatrix();

    // --- Ametralladoras en las alas ---
    glColor3f(0.1f, 0.1f, 0.1f);
    // Izquierda
    glPushMatrix();
    glTranslatef(0.5f, -0.38f, 0.0f);
    glRotatef(90, 0, 1, 0);
    gluCylinder(quad, 0.018, 0.018, 0.18, 8, 1);
    glPopMatrix();
    // Derecha
    glPushMatrix();
    glTranslatef(0.5f, 0.38f, 0.0f);
    glRotatef(90, 0, 1, 0);
    gluCylinder(quad, 0.018, 0.018, 0.18, 8, 1);
    glPopMatrix();
    glPopMatrix();

    gluDeleteQuadric(quad);
}

// Función para dibujar texto en la ventana
void dibujarTexto(float x, float y, const char* texto) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    gluOrtho2D(0, w, 0, h);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glRasterPos2f(x, y);

    for (const char* c = texto; *c; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Dibujar fondo primero
    dibujarFondoImagen();

    gluLookAt(0.0, 0.0, 2.0,   // Posición de la cámara
        0.0, 0.0, 0.0,   // Hacia dónde mira
        0.0, 1.0, 0.0);  // Vector "arriba"

    dibujarAvion3D();
    dibujarBalas();
    dibujarFaros();
    dibujarEnemigo();

    // Obtiene y muestra la información del avión
    std::string direccion = getDireccionCardinal(rotacion);
    std::stringstream ss;
    ss << "Dirección: " << direccion;
    ss << " | Posición: (" << posX << ", " << posY << ")";
    ss << " | Rotación: " << rotacion << "°";
    if (farosEncendidos) ss << " | Faros: ENCENDIDOS";
    else ss << " | Faros: APAGADOS";

    // Información del enemigo - VIDA EN NÚMEROS GRANDES
    std::stringstream enemyInfo;
    if (enemigo.activo) {
        enemyInfo << "ENERGIA RESTANTE: " << enemigo.energiaRestante;
    }
    else {
        enemyInfo << "ENEMIGO DESTRUIDO - Reapareciendo...";
    }

    dibujarTexto(10, 20, ss.str().c_str());
    dibujarTexto(10, 40, "Controles: W/S - Avanzar/Retroceder, A/D - Rotar, R - Disparar, T - Faros");

    // Dibujar la vida del enemigo 
    dibujarTexto(10, 80, enemyInfo.str().c_str());

    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(FOV_Y_DEGREES, (float)w / (float)h, 0.1, 10.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int x, int y) {
    keys[key] = true;

    
    switch (key) {
    case 'r':
    case 'R':
        disparar();
        break;
    case 't':
    case 'T':
        farosEncendidos = !farosEncendidos;
        break;
    }
    glutPostRedisplay();
}

void keyboardUp(unsigned char key, int x, int y) {
    keys[key] = false;
}

void idle() {

    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    deltaTime = (currentTime - lastFrameTime) / 1000.0f;
    lastFrameTime = currentTime;

    // Limita delta time para evitar saltos grandes
    if (deltaTime > 0.1f) deltaTime = 0.1f;

    // Procesa teclas pulsadas simultáneamente
    float velocidadMovimiento = 1.0f; // Unidades por segundo
    float velocidadRotacion = 90.0f;  // Grados por segundo

    // Movimiento adelante/atrás
    if (keys['w'] || keys['W']) {
        float dirX = cos(toRadians(rotacion));
        float dirY = sin(toRadians(rotacion));
        posX += dirX * velocidadMovimiento * deltaTime;
        posY += dirY * velocidadMovimiento * deltaTime;
    }
    if (keys['s'] || keys['S']) {
        float dirX = cos(toRadians(rotacion));
        float dirY = sin(toRadians(rotacion));
        posX -= dirX * velocidadMovimiento * deltaTime;
        posY -= dirY * velocidadMovimiento * deltaTime;
    }

    // Rotación
    if (keys['a'] || keys['A']) {
        rotacion += velocidadRotacion * deltaTime;
        // Normalizar rotación a 0-360
        if (rotacion >= 360.0f) rotacion -= 360.0f;
    }
    if (keys['d'] || keys['D']) {
        rotacion -= velocidadRotacion * deltaTime;
        // Normalizar rotación a 0-360
        if (rotacion < 0.0f) rotacion += 360.0f;
    }

    // Limitar posición del avión con límites dinámicos y márgenes del propio avión
    limitarDentroPantalla(posX, posY, AVION_MARGEN_X, AVION_MARGEN_Y);

    // Actualiza balas y enemigo
    moverBalas(deltaTime);
    actualizarEnemigo(deltaTime);

    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Avión 3D con Enemigo y Combate");


    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Color de fondo
    glClearColor(0.0f, 0.0f, 0.0f, 0.1f);

   
    cargarTexturaFondo("C:/Users/Equipo/Documents/avion/assets/espacio.bmp");

    // Registrar callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutIdleFunc(idle);

    // Inicializar sistema de balas y tiempo
    inicializarBalas();
    lastFrameTime = glutGet(GLUT_ELAPSED_TIME);

    glutMainLoop();
    return 0;
}