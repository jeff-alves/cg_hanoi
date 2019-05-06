#include <GL/gl.h>
#include <GL/glut.h>

#include <cmath>
#include <iostream>
#include <list>

using namespace std;

// Menu items
enum MENU_TYPE
{
    M_NONE,
    MENU_HELP,
    MENU_SOLVE,
    MENU_INCREASE_SPEED,
    MENU_DECREASE_SPEED,
    M_PAUSE,
    LIGHT_AUTO_MOTION,
    M_POSITIONAL,
    M_DIRECTIONAL,
    MENU_FULL_SCREEN,
    MENU_Exit
};

class CustomPoint {
public:
    double x;
    double y;
    double z;
    CustomPoint()
    {
        x = 0.0;
        y = 0.0;
        z = 0.0;
    }
    CustomPoint(double Set_X, double Set_Y, double Set_Z)
    {
        x = Set_X;
        y = Set_Y;
        z = Set_Z;
    }
    CustomPoint& operator= (CustomPoint const& obj)
    {
        x = obj.x;
        y = obj.y;
        z = obj.z;
        return *this;
    }
    CustomPoint operator-(CustomPoint const& p1)
    {
        return CustomPoint(x - p1.x, y - p1.y, z - p1.z);
    }
};

class Disk {
public:
    Disk()
    {
        normal = CustomPoint(0.0, 0.0, 1.0);
    }
    CustomPoint position; //
    CustomPoint normal;   //orientation
};

struct ActiveDisc {    //Active Disc to be moved [later in motion]
    int disc_index;
    CustomPoint start_pos, dest_pos;
    double u;		    // u E [0, 1]
    double step_u;
    bool is_in_motion;
    int direction;     // +1 for Left to Right & -1 for Right to left, 0 = stationary
};

// Axis and Discs Globals - Can be changed for different levels
const size_t NUM_DISCS = 6;
const double AXIS_HEIGHT = 3.0;

struct Axis {
    CustomPoint positions[NUM_DISCS];
    int occupancy_val[NUM_DISCS];
};

struct GameBoard {
    double x_min, y_min, x_max, y_max; //Base in XY-Plane
    double axis_base_rad;               //Axis's base radius
    Axis axis[3];
};

struct solution_pair {
    size_t f, t;         //f = from, t = to
};

//Game Settings
Disk discs[NUM_DISCS];
GameBoard t_board;
ActiveDisc active_disc;
list<solution_pair> sol;
bool to_solve = false;

//Globals for window, time, FPS
double FOV = 45.0;
size_t FPS = 60;
size_t prev_time = 0;
size_t window_width = 600, window_height = 600;

void initialize();
void initialize_game();
void display_handler();
void reshape_handler(int w, int h);
void keyboard_handler(unsigned char key, int x, int y);
void DrawAxe(double x, double y, double r, double h);
void anim_handler();
void mouseWheel(int dir);
void visible(int vis);
void toggleFullScreen();
void move_disc(int from_axis, int to_axis);
CustomPoint get_inerpolated_coordinate(CustomPoint v1, CustomPoint v2, double u);
void move_stack(int n, int f, int t); // Hanoi Algorithem
void menu(int); // Menu handling function declaration
int main(int argc, char** argv);


/* Variable controlling various rendering modes. */
int animation = 1;
int directionalLight = 1;
int fullScreen = 1;

/* Time varying or user-controled variables. */
float lightAngle = 0.67, lightHeight = 10;
GLfloat angle = 0;   /* in degrees */
GLfloat angle2 = 30;   /* in degrees */

int moving, startx, starty;
int lightManualMoving = 0, lightStartX, lightStartY;
int lightAutoMove = 0;

GLfloat lightPosition[4];
GLfloat lightColor[] = {1.0, 1.0, 1.0, 1.0};

enum {
    X, Y, Z, W
};
enum {
    A, B, C, D
};

/* Create a matrix that will project the desired shadow. */
void shadowMatrix(GLfloat shadowMat[4][4], GLfloat groundplane[4], GLfloat lightpos[4])
{
    GLfloat dot;
    /* Find dot product between light position vector and ground plane normal. */
    dot = groundplane[X] * lightpos[X] +
          groundplane[Y] * lightpos[Y] +
          groundplane[Z] * lightpos[Z] +
          groundplane[W] * lightpos[W];

    shadowMat[0][0] = dot - lightpos[X] * groundplane[X];
    shadowMat[1][0] = 0.f - lightpos[X] * groundplane[Y];
    shadowMat[2][0] = 0.f - lightpos[X] * groundplane[Z];
    shadowMat[3][0] = 0.f - lightpos[X] * groundplane[W];

    shadowMat[X][1] = 0.f - lightpos[Y] * groundplane[X];
    shadowMat[1][1] = dot - lightpos[Y] * groundplane[Y];
    shadowMat[2][1] = 0.f - lightpos[Y] * groundplane[Z];
    shadowMat[3][1] = 0.f - lightpos[Y] * groundplane[W];

    shadowMat[X][2] = 0.f - lightpos[Z] * groundplane[X];
    shadowMat[1][2] = 0.f - lightpos[Z] * groundplane[Y];
    shadowMat[2][2] = dot - lightpos[Z] * groundplane[Z];
    shadowMat[3][2] = 0.f - lightpos[Z] * groundplane[W];

    shadowMat[X][3] = 0.f - lightpos[W] * groundplane[X];
    shadowMat[1][3] = 0.f - lightpos[W] * groundplane[Y];
    shadowMat[2][3] = 0.f - lightpos[W] * groundplane[Z];
    shadowMat[3][3] = dot - lightpos[W] * groundplane[W];

}

/* Setup floor plane for projected shadow calculations. */
/* Find the plane equation given 3 points. */
void findPlane(GLfloat plane[4], GLfloat v0[3], GLfloat v1[3], GLfloat v2[3])
{
    GLfloat vec0[3], vec1[3];

    /* Need 2 vectors to find cross product. */
    vec0[X] = v1[X] - v0[X];
    vec0[Y] = v1[Y] - v0[Y];
    vec0[Z] = v1[Z] - v0[Z];

    vec1[X] = v2[X] - v0[X];
    vec1[Y] = v2[Y] - v0[Y];
    vec1[Z] = v2[Z] - v0[Z];

    /* find cross product to get A, B, and C of plane equation */
    plane[A] = vec0[Y] * vec1[Z] - vec0[Z] * vec1[Y];
    plane[B] = -(vec0[X] * vec1[Z] - vec0[Z] * vec1[X]);
    plane[C] = vec0[X] * vec1[Y] - vec0[Y] * vec1[X];

    plane[D] = -(plane[A] * v0[X] + plane[B] * v0[Y] + plane[C] * v0[Z]);
}

GLfloat floorVertices[4][3] = {
        { -16.0, 0.0, 16.0 },
        { 16.0, 0.0, 16.0 },
        { 16.0, 0.0, -16.0 },
        { -16.0, 0.0, -16.0 },
};

/* Draw a floor (possibly textured). */
void drawFloor(void)
{
    glDisable(GL_LIGHTING);
    glBegin(GL_QUADS);
    glVertex3fv(floorVertices[0]);
    glVertex3fv(floorVertices[1]);
    glVertex3fv(floorVertices[2]);
    glVertex3fv(floorVertices[3]);
    glEnd();
    glEnable(GL_LIGHTING);
}

GLfloat floorPlane[4];
GLfloat floorShadow[4][4];

/* ARGSUSED2 */
void mouse(int button, int state, int x, int y)
{
    switch(button) {
        case GLUT_LEFT_BUTTON:
            if (state == GLUT_DOWN) {
                moving = 1;
                startx = x;
                starty = y;
            }
            if (state == GLUT_UP) {
                moving = 0;
            }
            break;
        case GLUT_MIDDLE_BUTTON:
            if (state == GLUT_DOWN) {
                lightManualMoving = 1;
                lightStartX = x;
                lightStartY = y;
            }
            if (state == GLUT_UP) {
                lightManualMoving = 0;
            }
            break;
        case 3:  //mouse wheel scrolls
            mouseWheel(0);
            break;
        case 4:
            mouseWheel(1);
            break;
    }
}

void motion(int x, int y)
{
    if (moving) {
        angle = angle + (x - startx);
        angle2 = angle2 + (y - starty);
        startx = x;
        starty = y;
        glutPostRedisplay();
    }
    if (lightManualMoving) {
        lightAngle += (x - lightStartX)/80.0;
        lightHeight += (lightStartY - y)/40.0;
        lightStartX = x;
        lightStartY = y;
        glutPostRedisplay();
    }
}

void mouseWheel(int dir)
{
    if (dir > 0)
    {
        if (FOV > 99) FPS = 100;
        else FOV += 1;
        cout << "(+) FOV " << endl;
    }
    else
    {
        if (FOV <= 10) FPS = 10;
        else FOV -= 1;
        cout << "(-) FOV " << endl;
    }
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(
            /* field of view in degree */ FOV,
            /* aspect ratio */   (GLfloat)window_width/(GLfloat)window_height,
            /* Z near */                  1.0,
            /* Z far */                  100.0
    );
    glMatrixMode(GL_MODELVIEW);
    glutPostRedisplay();
}

void special(int k, int x, int y)
{
    glutPostRedisplay();
}

void print_info()
{
    cout << "Torres de Hanoi" << endl;
    cout << "H:\t\tAjuda" << endl;
    cout << "ESC:\tSair" << endl;
    cout << "S:\t\tStart" << endl;
    cout << "+/-:\tControla velocidade" << endl;
    cout << "-----------------------------" << endl;
    cout << "Grupo:" << endl;
    cout << "\tJefferson Alves" << endl;
    cout << "\tLucas Oliveira" << endl;
    cout << "-----------------------------" << endl;
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH | GLUT_STENCIL | GLUT_MULTISAMPLE);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Torres de Hanoi");
    glutFullScreen();
    print_info();

    /* Register GLUT callbacks. */
    glutDisplayFunc(display_handler);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutVisibilityFunc(visible);
    glutReshapeFunc(reshape_handler);
    glutKeyboardFunc(keyboard_handler);
    glutIdleFunc(anim_handler);
    glutSpecialFunc(special);

    initialize_game();  //Initializing Game State
    initialize();       //Initializing OpenGL

    // Create a menu
    glutCreateMenu(menu);
    glutAddMenuEntry("Help (H)", MENU_HELP);
    glutAddMenuEntry("Solve (S)", MENU_SOLVE);
    glutAddMenuEntry("Increase Speed (+)", MENU_INCREASE_SPEED);
    glutAddMenuEntry("Decrease Speed (-)", MENU_DECREASE_SPEED);
    glutAddMenuEntry("----------------------", M_NONE);
    glutAddMenuEntry("Toggle pause", M_PAUSE);
    glutAddMenuEntry("Toggle light auto motion", LIGHT_AUTO_MOTION);
    glutAddMenuEntry("----------------------", M_NONE);
    glutAddMenuEntry("Positional light", M_POSITIONAL);
    glutAddMenuEntry("Directional light", M_DIRECTIONAL);
    glutAddMenuEntry("Toggle fullscreen", MENU_FULL_SCREEN);
    glutAddMenuEntry("-----------------------", M_NONE);
    glutAddMenuEntry("Exit (Q, Esc)", MENU_Exit);
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    findPlane(floorPlane, floorVertices[1], floorVertices[2], floorVertices[3]);

    glutMainLoop();
    return 0;
}

void initialize()
{

    glPolygonOffset(-0.1, -0.1);

    glClearColor(0.1, 0.1, 0.1, 5.0); //Background Color
    glShadeModel(GL_SMOOTH);		  //SMOOTH Shading
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glLineWidth(3.0);


    glMatrixMode(GL_MODELVIEW);
    gluLookAt(0.0, 0.0, 30.0,  /* eye */
              0.0, 2.0, 0.0,      /* center */
              0.0, 1.0, 0.0);      /* up is in postivie Y direction */


    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 1);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.1);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.05);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);

    //Globals initializations
    prev_time = glutGet(GLUT_ELAPSED_TIME);
}

void initialize_game()
{
    //Initializing 1)GameBoard t_board 2) Discs discs  3) ActiveDisc active_disc
    // State

    //1) Initializing GameBoard
    t_board.axis_base_rad = 1.0;
    t_board.x_min = 0.0;
    t_board.x_max = 10 * t_board.axis_base_rad;
    t_board.y_min = 0.0;
    t_board.y_max = 3 * t_board.axis_base_rad;

    double x_center = 0;
    double y_center = 0;
    double dx = (t_board.x_max - t_board.x_min) / 3.0; //Since 3 Axis
//    double r = t_board.axis_base_rad;

    //Initializing axis Occupancy value
    for (size_t i = 0; i < 3; i++)
    {
        for (size_t h = 0; h < NUM_DISCS; h++)
        {
            if (i == 0)
            {
                t_board.axis[i].occupancy_val[h] = NUM_DISCS - 1 - h;
            }
            else t_board.axis[i].occupancy_val[h] = -1;
        }
    }

    //Initializing Axis positions
    for (size_t i = 0; i < 3; i++)
    {
        for (size_t h = 0; h < NUM_DISCS; h++)
        {
            double x = x_center + ((int)i - 1) * dx;
            double y = y_center;
            double z = (h + 1) * 0.3;
            CustomPoint& pos_to_set = t_board.axis[i].positions[h];
            pos_to_set.x = x;
            pos_to_set.y = y;
            pos_to_set.z = z;
        }
    }

    //2) Initializing Discs
    for (size_t i = 0; i < NUM_DISCS; i++)
    {
        discs[i].position = t_board.axis[0].positions[NUM_DISCS - i - 1];
    }
    //3) Initializing Active Disc
    active_disc.disc_index = -1;
    active_disc.is_in_motion = false;
    active_disc.step_u = 0.025;
    active_disc.u = 0.0;
    active_disc.direction = 0;
}

//Draw function for drawing a cylinder given position and radius and height
void DrawAxe(double x, double y, double r, double h)
{
    GLUquadric* q = gluNewQuadric();
    GLint slices = 50;
    GLint stacks = 10;
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    gluCylinder(q, r, r, h, slices, stacks);
    glTranslatef(0, 0, h);
    gluDisk(q, 0, r, slices, stacks);
    glPopMatrix();
    gluDeleteQuadric(q);
}

//Draw function for drawing axis on a given game board i.e. base
void DrawBoardAndAxis(GameBoard const& board)
{
    //Materials,
    GLfloat mat_yellow[] = { 1.0f, 1.0f, 0.0f, 1.0f };
    glPushMatrix();
    //Drawing axis and Pedestals
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_yellow);
    glRotatef(-90,1,0,0);
    double r = board.axis_base_rad;
    for (size_t i = 0; i < 3; i++)
    {
        CustomPoint const& p = board.axis[i].positions[0];
        DrawAxe(p.x, p.y, r * 0.1, AXIS_HEIGHT - 0.1);
        DrawAxe(p.x, p.y, r, 0.1);
    }
    glPopMatrix();
}

// Draw function for drawing discs
void draw_discs()
{
    int slices = 100;
    int stacks = 10;

    double rad;
    GLfloat r, g, b;
    GLfloat emission[] = { 0.4f, 0.4f, 0.4f, 1.0f };
    GLfloat no_emission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat material[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    for (size_t i = 0; i < NUM_DISCS; i++)
    {
        switch (i)
        {
            case 0: r = 1.0f; g = 0.0f; b = 0.0f;
                break;
            case 1: r = 0.0f; g = 1.0f; b = 0.0f;
                break;
            case 2: r = 0.0f, g = 0.0f; b = 1.0f;
                break;
            case 3: r = 1.0f, g = 1.0f; b = 0.0f;
                break;
            case 4: r = 0.0f, g = 1.0f; b = 1.0f;
                break;
            case 5: r = 1.0f, g = 0.0f; b = 1.0f;
                break;
            default: r = g = b = 1.0f;
                break;
        };

        material[0] = r;
        material[1] = g;
        material[2] = b;
        glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, material);

        GLfloat factor = 1.6f;
        switch (i) {
            case 0: factor = 0.2;
                break;
            case 1: factor = 0.4;
                break;
            case 2: factor = 0.6;
                break;
            case 3: factor = 0.8;
                break;
            case 4: factor = 1.0;
                break;
            case 5: factor = 1.2;
                break;
            case 6: factor = 1.4;
                break;
            default: break;
        };
        rad = factor * t_board.axis_base_rad;
        int d = active_disc.direction;

        glPushMatrix();
        glRotatef(-90,1,0,0);
        glTranslatef(discs[i].position.x, discs[i].position.y, discs[i].position.z);
        double theta = acos(discs[i].normal.z);
        theta *= 640.0f / M_PI;
        glRotatef(d * theta, 0.0f, 1.0f, 0.0f);
        glutSolidTorus(0.2 * t_board.axis_base_rad, rad, stacks, slices);
        glPopMatrix();

        glMaterialfv(GL_FRONT, GL_EMISSION, no_emission);
    }
}

void display_handler()
{

    /* Clear; default stencil clears to zero. */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    /* Reposition the light source. */
    lightPosition[0] = 12*cos(lightAngle);
    lightPosition[1] = lightHeight;
    lightPosition[2] = 12*sin(lightAngle);
    if (directionalLight) {
        lightPosition[3] = 0.0;
    } else {
        lightPosition[3] = 1.0;
    }

    shadowMatrix(floorShadow, floorPlane, lightPosition);

    glPushMatrix();
    /* Perform scene rotations based on user mouse input. */
    glRotatef(angle2, 1.0, 0.0, 0.0);
    glRotatef(angle, 0.0, 1.0, 0.0);

    /* Tell GL new light source position. */
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);

    /* We can eliminate the visual "artifact" of seeing the "flipped"
    dinosaur underneath the floor by using stencil.  The idea is
    draw the floor without color or depth update but so that
    a stencil value of one is where the floor will be.  Later when
    rendering the dinosaur reflection, we will only update pixels
    with a stencil value of 1 to make sure the reflection only
    lives on the floor, not below the floor. */

    /* Don't update color or depth. */
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    /* Draw 1 into the stencil buffer. */
    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glStencilFunc(GL_ALWAYS, 1, 0xffffffff);

    /* Now render floor; floor pixels just get their stencil set to 1. */
    drawFloor();

    /* Re-enable update of color and depth. */
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    /* Now, only render where stencil is set to 1. */
    glStencilFunc(GL_EQUAL, 1, 0xffffffff);  /* draw if ==1 */
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);


    glPushMatrix();

    /* The critical reflection step: Reflect dinosaur through the floor
       (the Y=0 plane) to make a relection. */
    glScalef(1.0, -1.0, 1.0);

    /* Reflect the light position. */
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);

    /* To avoid our normals getting reversed and hence botched lighting
    on the reflection, turn on normalize.  */
    glEnable(GL_NORMALIZE);
    glCullFace(GL_FRONT);

    /* Draw the reflected dinosaur. */
//    glRotatef(-90, 1, 0, 0);
    DrawBoardAndAxis(t_board);
    draw_discs();
//    glRotatef(90, 1, 0, 0);

    /* Disable noramlize again and re-enable back face culling. */
    glDisable(GL_NORMALIZE);
    glCullFace(GL_BACK);

    glPopMatrix();

    /* Switch back to the unreflected light position. */
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);

    glDisable(GL_STENCIL_TEST);

    /* Back face culling will get used to only draw either the top or the
       bottom floor.  This let's us get a floor with two distinct
       appearances.  The top floor surface is reflective and kind of red.
       The bottom floor surface is not reflective and blue. */

    /* Draw "bottom" of floor in blue. */
    glFrontFace(GL_CW);  /* Switch face orientation. */
    glColor4f(0.3, 0.3, 0.3, 1.0);
    drawFloor();
    glFrontFace(GL_CCW);

    /* Draw the floor with stencil value 3.  This helps us only
       draw the shadow once per floor pixel (and only on the
       floor pixels). */
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 3, 0xffffffff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    /* Draw "top" of floor.  Use blending to blend in reflection. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0, 1.0, 1.0, 0.3);
    drawFloor();
    glDisable(GL_BLEND);

    /* Draw "actual" dinosaur, not its reflection. */
//    glRotatef(-90, 1, 0, 0);
    DrawBoardAndAxis(t_board);
    draw_discs();
//    glRotatef(90, 1, 0, 0);

    /* Render the projected shadow. */

    /* Now, only render where stencil is set above 2 (ie, 3 where
    the top floor is).  Update stencil with 2 where the shadow
    gets drawn so we don't redraw (and accidently reblend) the
    shadow). */
    glStencilFunc(GL_LESS, 2, 0xffffffff);  /* draw if ==1 */
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);

    /* To eliminate depth buffer artifacts, we use polygon offset
    to raise the depth of the projected shadow slightly so
    that it does not depth buffer alias with the floor. */
    glEnable(GL_POLYGON_OFFSET_EXT);

    /* Render 50% black shadow color on top of whatever the
     floor appareance is. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);  /* Force the 50% black. */
    glColor4f(0.0, 0.0, 0.0, 0.5);

    glPushMatrix();
    /* Project the shadow. */
    glMultMatrixf((GLfloat *) floorShadow);

//    glRotatef(-90, 1, 0, 0);
    DrawBoardAndAxis(t_board);
    draw_discs();
//    glRotatef(90, 1, 0, 0);

    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

    glDisable(GL_POLYGON_OFFSET_EXT);
    glDisable(GL_STENCIL_TEST);


    glPushMatrix();
    glDisable(GL_LIGHTING);
    glColor3f(1.0, 1.0, 0.0);
    if (directionalLight) {
        /* Draw an arrowhead. */
        glDisable(GL_CULL_FACE);
        glTranslatef(lightPosition[0], lightPosition[1], lightPosition[2]);
        glRotatef(lightAngle * -180.0 / M_PI, 0, 1, 0);
        glRotatef(atan(lightHeight/12) * 180.0 / M_PI, 0, 0, 1);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(0, 0, 0);
        glVertex3f(2, 1, 1);
        glVertex3f(2, -1, 1);
        glVertex3f(2, -1, -1);
        glVertex3f(2, 1, -1);
        glVertex3f(2, 1, 1);
        glEnd();
        /* Draw a white line from light direction. */
        glColor3f(1.0, 1.0, 1.0);
        glBegin(GL_LINES);
        glVertex3f(0, 0, 0);
        glVertex3f(5, 0, 0);
        glEnd();
        glEnable(GL_CULL_FACE);
    } else {
        /* Draw a yellow ball at the light source. */
        glTranslatef(lightPosition[0], lightPosition[1], lightPosition[2]);
        glutSolidSphere(1.0, 5, 5);
    }
    glEnable(GL_LIGHTING);
    glPopMatrix();

    glPopMatrix();

    glutSwapBuffers();
}

void reshape_handler(int w, int h)
{
    window_width = w;
    window_height = h;

    glViewport(0, 0, (GLsizei)w, (GLsizei)h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(
            /* field of view in degree */ FOV,
            /* aspect ratio */   (GLfloat)w/(GLfloat)h,
            /* Z near */                  1.0,
            /* Z far */                  100.0
    );

    glMatrixMode(GL_MODELVIEW);
}

void move_stack(int n, int f, int t)
{
    if (n == 1) {
        solution_pair s;
        s.f = f;
        s.t = t;
        sol.push_back(s); //pushing the (from, to) pair of solution to a list [so that it can be animated later]
        cout << "From Axis " << f << " to Axis " << t << endl;
        return;
    }
    move_stack(n - 1, f, 3 - t - f);
    move_stack(1, f, t);
    move_stack(n - 1, 3 - t - f, t);
}

void solve()
{
    move_stack(NUM_DISCS, 0, 2);
}

void keyboard_handler(unsigned char key, int x, int y)
{
    switch (key)
    {
        case 27:
        case 'q':
        case 'Q':
            exit(0);
            break;
        case 'h':
        case 'H':
            print_info();
            break;
        case 's':
        case 'S':
            if (t_board.axis[0].occupancy_val[NUM_DISCS - 1] < 0)
                break;
            solve();
            to_solve = true;
            break;
        case '+':
            if (FPS > 980) FPS = 1000;
            else FPS += 20;
            cout << "(+) Speed: " << FPS / 5.0 << "%" << endl;
            break;
        case '-':
            if (FPS <= 20) FPS = 2;
            else FPS -= 20;
            cout << "(-) Speed: " << FPS / 5.0 << "%" << endl;
            break;
        case 'f':
        case 'F':
            toggleFullScreen();
            break;
        default:
            break;
    };
    glutPostRedisplay();
}

void toggleFullScreen() {
    fullScreen = 1 - fullScreen;
    if (fullScreen) {
        glutFullScreen();
    } else {
        glutReshapeWindow(600, 600);
//        glutPositionWindow(0, 0);
    }
}

void move_disc(int from_axis, int to_axis)
{

    int d = to_axis - from_axis;
    if (d > 0) active_disc.direction = 1;
    else if (d < 0) active_disc.direction = -1;

    if ((from_axis == to_axis) || (from_axis < 0) || (to_axis < 0) || (from_axis > 2) || (to_axis > 2))
        return;

    size_t i;
    for (i = NUM_DISCS - 1; i >= 0 && t_board.axis[from_axis].occupancy_val[i] < 0; i--);
    if ((i < 0) || (i == 0 && t_board.axis[from_axis].occupancy_val[i] < 0))
        return; //Either the index < 0 or index at 0 and occupancy < 0 => it's an empty Axis

    active_disc.start_pos = t_board.axis[from_axis].positions[i];

    active_disc.disc_index = t_board.axis[from_axis].occupancy_val[i];
    active_disc.is_in_motion = true;
    active_disc.u = 0.0;


    size_t j;
    for (j = 0; j < NUM_DISCS - 1 && t_board.axis[to_axis].occupancy_val[j] >= 0; j++);
    active_disc.dest_pos = t_board.axis[to_axis].positions[j];

    t_board.axis[from_axis].occupancy_val[i] = -1;
    t_board.axis[to_axis].occupancy_val[j] = active_disc.disc_index;
}

CustomPoint get_inerpolated_coordinate(CustomPoint sp, CustomPoint tp, double u)
{
    //4 Control points
    CustomPoint p;
    double x_center = 0;
    double y_center = 0;

    double u3 = u * u * u;
    double u2 = u * u;

    CustomPoint cps[4]; //P1, P2, dP1, dP2


    //Hermite Interpolation [Check Reference for equation of spline]
    {
        //P1
        cps[0].x = sp.x;
        cps[0].y = y_center;
        cps[0].z = AXIS_HEIGHT + 0.2 * (t_board.axis_base_rad);

        //P2
        cps[1].x = tp.x;
        cps[1].y = y_center;
        cps[1].z = AXIS_HEIGHT + 0.2 * (t_board.axis_base_rad);

        //dP1
        cps[2].x = (sp.x + tp.x) / 2.0 - sp.x;
        cps[2].y = y_center;
        cps[2].z = 2 * cps[1].z; //change 2 * ..

        //dP2
        cps[3].x = tp.x - (tp.x + sp.x) / 2.0;
        cps[3].y = y_center;
        cps[3].z = -cps[2].z; //- cps[2].z;


        double h0 = 2 * u3 - 3 * u2 + 1;
        double h1 = -2 * u3 + 3 * u2;
        double h2 = u3 - 2 * u2 + u;
        double h3 = u3 - u2;

        p.x = h0 * cps[0].x + h1 * cps[1].x + h2 * cps[2].x + h3 * cps[3].x;
        p.y = h0 * cps[0].y + h1 * cps[1].y + h2 * cps[2].y + h3 * cps[3].y;
        p.z = h0 * cps[0].z + h1 * cps[1].z + h2 * cps[2].z + h3 * cps[3].z;

    }

    return p;
}

void normalize(CustomPoint& v)
{
    double length = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length == 0.0) return;
    v.x /= length;
    v.y /= length;
    v.z /= length;
}


void anim_handler()
{
    int curr_time = glutGet(GLUT_ELAPSED_TIME);
    int elapsed = curr_time - prev_time; // in ms
    if (elapsed < 1000 / FPS) return;

    prev_time = curr_time;

    if (!lightManualMoving && lightAutoMove) {
        lightAngle += 0.03;
    }

    if (to_solve && active_disc.is_in_motion == false) {
        solution_pair s = sol.front();

        cout << "From : " << s.f << " To -> " << s.t << endl;

        sol.pop_front();
        int i;
        for (i = NUM_DISCS; i >= 0 && t_board.axis[s.f].occupancy_val[i] < 0; i--);
        int ind = t_board.axis[s.f].occupancy_val[i];

        if (ind >= 0)
            active_disc.disc_index = ind;

        move_disc(s.f, s.t);
        if (sol.size() == 0)
            to_solve = false;
    }

    if (active_disc.is_in_motion)
    {
        int ind = active_disc.disc_index;
        ActiveDisc& ad = active_disc;

        if (ad.u == 0.0 && (discs[ind].position.z < AXIS_HEIGHT + 0.2 * (t_board.axis_base_rad)))
        {
            discs[ind].position.z += 0.05;
            glutPostRedisplay();
            return;
        }

        static bool done = false;
        if (ad.u == 1.0 && discs[ind].position.z > ad.dest_pos.z)
        {
            done = true;
            discs[ind].normal = CustomPoint(0, 0, 1);
            discs[ind].position.z -= 0.05;
            glutPostRedisplay();
            return;
        }

        ad.u += ad.step_u;
        if (ad.u > 1.0) {
            ad.u = 1.0;
        }

        if (!done) {
            CustomPoint prev_p = discs[ind].position;
            CustomPoint p = get_inerpolated_coordinate(ad.start_pos, ad.dest_pos, ad.u);
            discs[ind].position = p;
            discs[ind].normal.x = (p - prev_p).x;
            discs[ind].normal.y = (p - prev_p).y;
            discs[ind].normal.z = (p - prev_p).z;
            normalize(discs[ind].normal);
        }

        if (ad.u >= 1.0 && discs[ind].position.z <= ad.dest_pos.z) {
            discs[ind].position.z = ad.dest_pos.z;
            ad.is_in_motion = false;
            done = false;
            ad.u = 0.0;
            discs[ad.disc_index].normal = CustomPoint(0, 0, 1);
            ad.disc_index = -1;
        }
        glutPostRedisplay();
    } else if (lightAutoMove) {
        glutPostRedisplay();
    }
}

/* When not visible, stop animating.  Restart when visible again. */
void visible(int vis)
{
    if (vis == GLUT_VISIBLE) {
        if (animation)
            glutIdleFunc(anim_handler);
    } else {
        if (!animation)
            glutIdleFunc(NULL);
    }
}

// Menu handling function definition
void menu(int item)
{
    switch (item)
    {
        case M_NONE:
            return;
        case M_PAUSE:
            animation = 1 - animation;
            if (animation) {
                glutIdleFunc(anim_handler);
            } else {
                glutIdleFunc(NULL);
            }
            break;
        case LIGHT_AUTO_MOTION:
            lightAutoMove = 1 - lightAutoMove;
            break;
        case M_POSITIONAL:
            directionalLight = 0;
            break;
        case M_DIRECTIONAL:
            directionalLight = 1;
            break;
        case MENU_HELP:
            print_info();
            break;
        case MENU_SOLVE:
            if (t_board.axis[0].occupancy_val[NUM_DISCS - 1] < 0)
                break;
            solve();
            to_solve = true;
            break;
        case MENU_INCREASE_SPEED:
            if (FPS > 980) FPS = 1000;
            else FPS += 20;
            cout << "(+) Speed: " << FPS / 5.0 << "%" << endl;
            break;
        case MENU_DECREASE_SPEED:
            if (FPS <= 20) FPS = 2;
            else FPS -= 20;
            cout << "(-) Speed: " << FPS / 5.0 << "%" << endl;
            break;
        case MENU_FULL_SCREEN:
            toggleFullScreen();
            break;
        case MENU_Exit:
            exit(0);
            break;
        default:
            break;
    }
    glutPostRedisplay();
    return;
}