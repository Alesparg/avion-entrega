#include <GL/freeglut.h>
#include <cmath>
#include <string>
#include <sstream>

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

// --- Balas ---
struct Bala {
    float x, y, z;
    float dirX, dirY; // Dirección de la bala basada en la rotación del avión
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

// Dispara balas desde las ametralladoras en las alas
void disparar() {
    // Coordenadas relativas de las ametralladoras en las alas
    float offsetX = 0.5f; // punta del ala
    float offsetY = 0.38f; // lateral de cada ala
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

// Mover balas según su dirección
void moverBalas(float deltaTime) {
    float velocidad = 2.0f; // Unidades por segundo
    float distancia = velocidad * deltaTime;

    for (int i = 0; i < MAX_BALAS; ++i) {
        if (balas[i].activa) {
            balas[i].x += balas[i].dirX * distancia;
            balas[i].y += balas[i].dirY * distancia;

            // Si la bala está fuera de los límites de la pantalla
            if (balas[i].x > 5.0f || balas[i].x < -5.0f ||
                balas[i].y > 5.0f || balas[i].y < -5.0f) {
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

void dibujarFaros() {
    // Posicion relativas a las puntas de las alas
    float offsetX = 0.5f;
    float offsetY = 0.38f;
    float z = 0.05f;

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
    glutSolidSphere(0.03, 12, 12);

    // Haz de luz si está encendido
    if (farosEncendidos) {
        glColor4f(1.0f, 1.0f, 0.7f, 0.3f); 
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPushMatrix();
        glRotatef(rotacion, 0, 0, 1); 
        glBegin(GL_TRIANGLES);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glVertex3f(0.25f, -0.05f, -0.01f);
        glVertex3f(0.25f, 0.05f, 0.01f);
        glEnd();
        glPopMatrix();
        glDisable(GL_BLEND);
    }
    glPopMatrix();

    // Derecho
    glColor3f(1.0f, 1.0f, 1.0f);
    glPushMatrix();
    glTranslatef(rightLightX, rightLightY, z);
    glutSolidSphere(0.03, 12, 12);

    if (farosEncendidos) {
        glColor4f(1.0f, 1.0f, 0.7f, 0.3f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPushMatrix();
        glRotatef(rotacion, 0, 0, 1); 
        glBegin(GL_TRIANGLES);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glVertex3f(0.25f, 0.05f, -0.01f);
        glVertex3f(0.25f, -0.05f, 0.01f);
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

    
    gluLookAt(0.0, 0.0, 2.0,   // Posición de la cámara
        0.0, 0.0, 0.0,   // Hacia dónde mira
        0.0, 1.0, 0.0);  // Vector "arriba"

    dibujarAvion3D();
    dibujarBalas();
    dibujarFaros();

    // Obtiene y muestra la información del avión
    std::string direccion = getDireccionCardinal(rotacion);
    std::stringstream ss;
    ss << "Dirección: " << direccion;
    ss << " | Posición: (" << posX << ", " << posY << ")";
    ss << " | Rotación: " << rotacion << "°";
    if (farosEncendidos) ss << " | Faros: ENCENDIDOS";
    else ss << " | Faros: APAGADOS";

    dibujarTexto(10, 20, ss.str().c_str());
    dibujarTexto(10, 40, "Controles: W/S - Avanzar/Retroceder, A/D - Rotar, R - Disparar, T - Faros");

    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (float)w / (float)h, 0.1, 10.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int x, int y) {
    keys[key] = true;

    // Teclas que funcionan como toggle (una pulsación)
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

    // Actualiza balas
    moverBalas(deltaTime);

    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Avión 3D con Rotación");

    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Color de fondo
    glClearColor(0.0f, 0.0f, 0.0f, 0.1f); 

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

