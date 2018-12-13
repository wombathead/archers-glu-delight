#include <stdio.h>
#include <math.h>
#include <iostream>

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#define WIN_W 640
#define WIN_H 640
#define FRAME_RATE 60

const int FRAME_INTERVAL = 1 * 1000 / FRAME_RATE;

int init();
void idle();
void display();
void keyboard(unsigned char, int, int);
void special(int, int, int);
void mouse_click(int, int, int, int);
void mouse_motion(int, int);
void reshape(int, int);

void camera_see();

void cross(const float *a, const float *b, float *n) {
    n[0] = (a[1] * b[2]) - (a[2] * b[1]);
    n[1] = (a[2] * b[0]) - (a[0] * b[2]);
    n[2] = (a[0] * b[1]) - (a[1] * b[0]);
}

bool first_mouse = true;    // avoid jump when first entering
bool warped = false;        // avoid jump when warping mouse back
bool escape_mouse = false;

bool english = false;   // weapon is teapot

// keep track of last mouse position
int last_x = 320;
int last_y = 320;

size_t g_target;    // target
size_t g_earth;     // the ground

clock_t prev_tick, curr_tick;
float dt;

enum bow_parts {
    HANDLE = 0,
    LIMB = 1,
    TIP = 2,
    STRING = 3,
    NUM_PARTS = 4
};

size_t g_bow;
float bow_handle_len = 0.4f;
float bow_limb_len = 0.2f;
float bow_tip_len = 0.1f;
float bow_curve = 15.0f;
float bow_str_len;

struct light_t {
    size_t name;
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float position[4];
};

struct material_t {
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float shininess;
};

const material_t brass = {
    {0.33f, 0.22f, 0.03f, 1.0f},
    {0.78f, 0.57f, 0.11f, 1.0f},
    {0.99f, 0.91f, 0.81f, 1.0f},
    27.8f
};

const material_t porcelain = {
    {0.1f, 0.1f, 0.1f, 1.0f},
    {0.5f, 0.5f, 0.5f, 1.0f},
    {0.5f, 0.5f, 0.5f, 1.0f},
    100.f
};

const material_t flat = {
    {0.2f, 0.2f, 0.2f, 1.0f},
    {0.2f, 0.2f, 0.2f, 1.0f},
    {0.2f, 0.2f, 0.2f, 1.0f},
    100
};

light_t white_light = {
    GL_LIGHT0,
    {0.0f, 0.0f, 0.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {0.0f, 5.0f, 0.0f, 1.0f}
};

light_t red_light = {
    GL_LIGHT1,
	{0.0f, 0.0f, 0.0f, 1.0f},
	{1.0f, 0.0f, 0.0f, 1.0f},
	{1.0f, 1.0f, 1.0f, 1.0f},
	{0.0f, 0.75f, 0.5f, 1.0f}
};

void set_material(const material_t &mat) {
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat.ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat.diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat.specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat.shininess);
};

void set_light(const light_t &light) {
    glLightfv(light.name, GL_AMBIENT, light.ambient);
    glLightfv(light.name, GL_DIFFUSE, light.diffuse);
    glLightfv(light.name, GL_SPECULAR, light.specular);
    glLightfv(light.name, GL_POSITION, light.position);

    glEnable(light.name);
}

struct vec3 {
    float x, y, z;
    vec3 operator*(const float& k) {
        struct vec3 res;
        res.x = x * k;
        res.y = y * k;
        res.z = z * k;
        return res;
    }
    vec3& operator+=(const vec3& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }
    vec3& operator+=(const float k) {
        x += k;
        y += k;
        z += k;
        return *this;
    }
    vec3& operator*=(const float k) {
        x *= k;
        y *= k;
        z *= k;
        return *this;
    }
    /*
    std::ostream& operator<<(std::ostream &strm, const vec3 &v) {
        return strm << "<" << v.x << ", " << v.y << "," << v.z << ">";
    }
    */
};

vec3 gravity = {0, -9.81, 0};

struct projectile_t {
    vec3 pos;
    vec3 vel;
};

struct camera_t {
    vec3 pos;
    float reference[3];
    float up[3];

    float pitch;
    float yaw;
    float roll;
};

projectile_t projectile = {
    {0, 0, 0},
    {0, 0, 0}
};

camera_t camera = {
    {0, 2, 10},
    {0, 0, 0},
    {0, 1, 0},

    0.0f, 0.0f, 0.0f
};

void draw_capped_cylinder(const float r, const float h, const int slices=32, const int stacks=32) {
    GLUquadricObj *obj = gluNewQuadric();
    gluQuadricNormals(obj, GLU_SMOOTH);

    gluCylinder(obj, r, r, h, slices, stacks);

    // top cap
    glPushMatrix();
        glTranslatef(0, 0, h);
        gluDisk(obj, 0, r, slices, stacks);
    glPopMatrix();

    // bottom cap
    glPushMatrix();
        glRotatef(180, 1, 0, 0);
        gluDisk(obj, 0, r, slices, stacks);
    glPopMatrix();
}

size_t make_target() {

    size_t handle = glGenLists(1);

    glNewList(handle, GL_COMPILE);
        draw_capped_cylinder(2.0f, 0.1f, 32, 32);
    glEndList();

    return handle;
}

size_t make_bow() {

    size_t handle = glGenLists(NUM_PARTS);

    glNewList(handle + HANDLE, GL_COMPILE);
        glPushMatrix();
            glRotatef(-90, 1, 0, 0);
            draw_capped_cylinder(0.01, bow_handle_len);
        glPopMatrix();
    glEndList();

    glNewList(handle + LIMB, GL_COMPILE);
        glPushMatrix();
            glRotatef(-90, 1, 0, 0);
            draw_capped_cylinder(0.01, bow_limb_len);
        glPopMatrix();
    glEndList();

    glNewList(handle + TIP, GL_COMPILE);
        glPushMatrix();
            glRotatef(-90, 1, 0, 0);
            draw_capped_cylinder(0.01, bow_tip_len);
        glPopMatrix();
    glEndList();

    glNewList(handle + STRING, GL_COMPILE);
        glPushMatrix();
            glRotatef(-90, 1, 0, 0);
            draw_capped_cylinder(0.005, bow_str_len);
        glPopMatrix();
    glEndList();

    return handle;
}

size_t make_earth() {

    const float verts[4][3] = {
        { 1, 0, 1 },
        { 1, 0, -1 },
        { -1, 0, -1 },
        { -1, 0, 1 }
    };

    float norm[3];
    cross(verts[0], verts[1], norm);

    size_t handle = glGenLists(1);

    glNewList(handle, GL_COMPILE);
        glNormal3f(norm[0], norm[1], norm[2]);
        glBegin(GL_QUADS);
            for (size_t i = 0; i < 4; i++)
                glVertex3fv(verts[i]);
        glEnd();
    glEndList();

    return handle;
}        

int main(int argc, char *argv[]) {

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);

    glutInitWindowSize(640, 640);
    glutInitWindowPosition(100, 100);

    glutCreateWindow("Archery");

    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse_click);
    glutPassiveMotionFunc(mouse_motion);
    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    // glutIdleFunc(idle);

    init();

    glutMainLoop();

    return 0;
}

int init() {

    set_light(white_light);

    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);

    glutSetCursor(GLUT_CURSOR_NONE);

    bow_str_len = 2 * (
            bow_handle_len / 2 + 
            bow_limb_len * cos(bow_curve * M_PI / 180) + 
            bow_tip_len * cos(2 * bow_curve * M_PI / 180)
    );

    g_target = make_target();
    g_earth = make_earth();
    g_bow = make_bow();

    return 0;
}

void simulate_physics() {

    prev_tick = curr_tick;
    curr_tick = clock();

    dt = ((float) (curr_tick - prev_tick)) / CLOCKS_PER_SEC;

    vec3 dv = gravity * dt;

    projectile.vel += dv;

    vec3 diff = projectile.vel * dt;
    projectile.pos += projectile.vel;

    if (projectile.pos.y <= 0) {
        projectile.pos.y = 0;
        projectile.vel.y *= -0.5;

        projectile.vel.x *= 0.8;
        projectile.vel.z *= 0.8;
    }

    glutPostRedisplay();
}

void idle() {
    simulate_physics();
}

void draw_weapon() {

    float down = -(bow_limb_len * cos(bow_curve * M_PI / 180) + bow_tip_len * cos(2 * bow_curve * M_PI / 180));
    float back = -(bow_limb_len * sin(bow_curve * M_PI / 180) + bow_tip_len * sin(2 * bow_curve * M_PI / 180));

    glPushMatrix();

    if (!english) {
        glTranslatef(0.4, -0.2, -1);
        glRotatef(15, 0, 1, 0);
        glCallList(g_bow + HANDLE);

        // draw top half of bow
        glPushMatrix();
            glTranslatef(0, bow_handle_len, 0);
            glRotatef(bow_curve, 1, 0, 0);
            glCallList(g_bow + LIMB);

            glPushMatrix();
                glTranslatef(0, bow_limb_len, 0);
                glRotatef(bow_curve, 1, 0, 0);
                glCallList(g_bow + TIP);
            glPopMatrix();
        glPopMatrix();

        glRotatef(180, 1, 0, 0);

        // draw bottom half of bow
        glPushMatrix();
            glTranslatef(0, 0, 0);
            glRotatef(-bow_curve, 1, 0, 0);
            glCallList(g_bow + LIMB);

            glPushMatrix();
                glTranslatef(0, bow_limb_len, 0);
                glRotatef(-bow_curve, 1, 0, 0);
                glCallList(g_bow + TIP);
            glPopMatrix();
        glPopMatrix();

        // draw bow string
        glPushMatrix();
            glTranslatef(0, -bow_str_len - down, back);
            glCallList(g_bow + STRING);
        glPopMatrix();
    } else {
        glTranslatef(0.4, -0.3, -0.7);
        glRotatef(90, 0, 1, 0);
        set_material(porcelain);
        glutSolidTeapot(0.15);
    }

    glPopMatrix();
}
        


void display() {

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(75, 1, 0.4, 100);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);

    glPushMatrix();

        // draw weapon
        draw_weapon();

        // translate everything to camera position/view
        camera_see();
        
        // draw the target
        glPushMatrix();
            glRotatef(90, 0, 1, 0);
            glTranslatef(0, 2, 0);
            glScalef(0.5f, 0.5f, 0.5f);
            set_material(brass);
            glCallList(g_target);
        glPopMatrix();


        // simulate physics
        simulate_physics();

        glPushMatrix();
            glTranslatef(projectile.pos.x, projectile.pos.y, projectile.pos.z);
            glutSolidSphere(0.1, 128, 128);
        glPopMatrix();

        // draw the ground
        glPushMatrix();
            glScalef(25, 25, 25);
            set_material(flat);
            glCallList(g_earth);
        glPopMatrix();

    glPopMatrix();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIN_W, 0, WIN_H);

    glPointSize(2.0f);
    glBegin(GL_POINTS);
        glVertex2f(WIN_W / 2, WIN_H / 2);
    glEnd();

    glutSwapBuffers();
}

void keyboard(unsigned char k, int, int) {

    switch (k) {
    case 'q':
        exit(1);
    case 'w':
        camera.pos.x += 0.2 * sin(camera.yaw * M_PI / 180);
        camera.pos.z -= 0.2 * cos(camera.yaw * M_PI / 180);
        break;
    case 's':
        camera.pos.x -= 0.2 * sin(camera.yaw * M_PI / 180);
        camera.pos.z += 0.2 * cos(camera.yaw * M_PI / 180);
        break;
    case 'a':
        camera.pos.x -= 0.2 * sin((90+camera.yaw) * M_PI / 180);
        camera.pos.z += 0.2 * cos((90+camera.yaw) * M_PI / 180);
        break;
    case 'd':
        camera.pos.x += 0.2 * sin((90+camera.yaw) * M_PI / 180);
        camera.pos.z -= 0.2 * cos((90+camera.yaw) * M_PI / 180);
        break;
    case 'm':
        escape_mouse = !escape_mouse;
        if (escape_mouse)
            glutSetCursor(GLUT_CURSOR_INHERIT);
        else
            glutSetCursor(GLUT_CURSOR_NONE);
        break;
    case 't':
        english = !english;
        break;
    }

    glutPostRedisplay();
}

void special(int k, int, int) {

    switch (k) {
    case GLUT_KEY_LEFT:
        break;
    case GLUT_KEY_RIGHT:
        break;
    case GLUT_KEY_UP:
        break;
    case GLUT_KEY_DOWN:
        break;
    }

    glutPostRedisplay();
}

vec3 normalize(vec3 v) {
    float len = sqrt(pow(v.x, 2) + pow(v.y, 2) + pow(v.z, 2));
    vec3 norm = {
        v.x / len,
        v.y / len,
        v.z / len
    };
    return norm;
}

void camera_see() {
    glRotatef(camera.pitch, 1.0f, 0.0f, 0.0f);  // rotate around x for pitch
    glRotatef(camera.yaw, 0.0f, 1.0f, 0.0f);    // rotate around y for yaw

    // translate screen to position of camera
    glTranslatef(-camera.pos.x, -camera.pos.y, -camera.pos.z);
}

void mouse_click(int button, int state, int x, int y) {

    switch (button) {
    case GLUT_LEFT_BUTTON:
        if (state == GLUT_UP) {
            projectile.pos = {
                camera.pos.x + 0.4f,
                camera.pos.y - 0.3f,
                camera.pos.z - 0.7f
            };

            projectile.vel = {
                sin(camera.yaw * M_PI / 180),
                -sin(camera.pitch * M_PI / 180),
                -cos(camera.yaw * M_PI / 180)
            };
            projectile.vel *= 2.8;
        }
    }
}

void mouse_motion(int x, int y) {

    if (first_mouse) {
        camera.pitch = camera.yaw = camera.roll = 0;
        last_x = x;
        last_y = y;
        first_mouse = false;
    }

    if (warped) {
        last_x = x;
        last_y = y;
        warped = false;
    }

    int dx = x - last_x;
    int dy = y - last_y;

    last_x = x;
    last_y = y;

    float sensitivity = 1.0;
    dx *= sensitivity;
    dy *= sensitivity;

    camera.yaw += dx;
    camera.pitch += dy;

    if (camera.pitch < -90)
        camera.pitch = -90;

    if (camera.pitch > 90)
        camera.pitch = 90;

    glutPostRedisplay();

    if (last_x > 3 * WIN_W / 4 || last_x < WIN_W / 4 || 
            last_y > 3 * WIN_H / 4|| last_y < WIN_H / 4) {

        if (!escape_mouse) {
            warped = true;
            glutWarpPointer(WIN_W / 2, WIN_H / 2);
        }
    }
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(75, 1, 0.4, 100);
}