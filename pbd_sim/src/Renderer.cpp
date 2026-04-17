#include "Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cstdio>
#include <stdexcept>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---- Shaders ----
static const char* kPtVert = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uVP; uniform float uSize;
void main(){ gl_Position=uVP*vec4(aPos,1.0); gl_PointSize=uSize; }
)glsl";
static const char* kPtFrag = R"glsl(
#version 330 core
out vec4 O; uniform vec4 uColor;
void main(){
    vec2 c=gl_PointCoord-0.5; float d=dot(c,c);
    if(d>0.25)discard;
    O=vec4(uColor.rgb,uColor.a*(1.0-smoothstep(0.20,0.25,d)));
}
)glsl";
static const char* kLineVert = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uVP;
void main(){ gl_Position=uVP*vec4(aPos,1.0); }
)glsl";
static const char* kLineFrag = R"glsl(
#version 330 core
out vec4 O; uniform vec4 uColor;
void main(){ O=uColor; }
)glsl";
static const char* kFaceVert = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uVP;
out vec3 vPos;
void main(){ vPos=aPos; gl_Position=uVP*vec4(aPos,1.0); }
)glsl";
static const char* kFaceFrag = R"glsl(
#version 330 core
in vec3 vPos; out vec4 O;
uniform vec3 uLight;
uniform vec4 uTint;
void main(){
    vec3 N=normalize(cross(dFdx(vPos),dFdy(vPos)));
    float NL=dot(N,uLight);
    vec3 base=uTint.rgb;
    O=vec4(base*(0.35+0.65*abs(NL)), uTint.a);
}
)glsl";

// ============================================================
Renderer::Renderer()=default;
Renderer::~Renderer()
{
    glDeleteVertexArrays(1,&m_ptVAO);   glDeleteBuffers(1,&m_ptVBO);
    glDeleteVertexArrays(1,&m_lineVAO); glDeleteBuffers(1,&m_lineVBO);
    glDeleteVertexArrays(1,&m_faceVAO); glDeleteBuffers(1,&m_faceVBO);
    glDeleteBuffers(1,&m_faceEBO);
    glDeleteProgram(m_ptShader);
    glDeleteProgram(m_lineShader);
    glDeleteProgram(m_faceShader);
}
void Renderer::init(int w,int h)
{
    buildShaders(); buildBuffers(); resize(w,h);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}
void Renderer::resize(int w,int h)
{ m_vpW=w;m_vpH=h;m_aspect=h>0?(float)w/h:1.f;glViewport(0,0,w,h); }

glm::mat4 Renderer::makeVP() const
{ float hH=8.f,hW=hH*m_aspect; return glm::ortho(-hW,hW,-hH,hH,-10.f,10.f); }

glm::vec2 Renderer::screenToWorld(double sx,double sy) const
{ float hH=8.f,hW=hH*m_aspect;
  return{((float)sx/m_vpW*2.f-1.f)*hW,-((float)sy/m_vpH*2.f-1.f)*hH}; }

// ============================================================
//  Main draw
// ============================================================
void Renderer::draw(const Simulation& sim)
{
    glClearColor(0.06f,0.06f,0.09f,1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    const auto& cfg = sim.config();
    glm::mat4   VP  = makeVP();
    float hW = 8.f * m_aspect;

    drawFloor(cfg.floorY, hW, VP);
    if (cfg.sphereEnabled) drawSphere(sim.sphere(), VP);
    if (cfg.mode == 3)     drawClothMesh(sim, VP);
    if (cfg.mode == 4 || cfg.mode == 5) drawRigidBodies(sim, VP);
    if (cfg.mode != 3 || showConstraints) drawConstraintLines(sim, VP);
    drawDragLine(sim, VP);
    if (showParticles) drawParticles(sim, VP);
}

// ============================================================
//  Draw helpers
// ============================================================

void Renderer::drawFloor(float floorY, float hW, const glm::mat4& VP)
{
    glUseProgram(m_lineShader);
    glUniformMatrix4fv(glGetUniformLocation(m_lineShader,"uVP"),1,GL_FALSE,glm::value_ptr(VP));
    glUniform4f(glGetUniformLocation(m_lineShader,"uColor"),0.28f,0.28f,0.36f,1.f);
    float v[]={-hW,floorY,0.f,hW,floorY,0.f};
    uploadLines({v,v+6});
    glDrawArrays(GL_LINES,0,2);
}

void Renderer::drawSphere(const SphereCollider& s, const glm::mat4& VP)
{
    const int segs=64;
    glUseProgram(m_lineShader);
    glUniformMatrix4fv(glGetUniformLocation(m_lineShader,"uVP"),1,GL_FALSE,glm::value_ptr(VP));
    glUniform4f(glGetUniformLocation(m_lineShader,"uColor"),0.55f,0.75f,0.95f,0.5f);
    std::vector<float> cv;
    for(int i=0;i<segs;++i)
    {
        float a0=2.f*(float)M_PI*i/segs, a1=2.f*(float)M_PI*(i+1)/segs;
        cv.push_back(s.center.x+cosf(a0)*s.radius);cv.push_back(s.center.y+sinf(a0)*s.radius);cv.push_back(0.f);
        cv.push_back(s.center.x+cosf(a1)*s.radius);cv.push_back(s.center.y+sinf(a1)*s.radius);cv.push_back(0.f);
    }
    uploadLines(cv);
    glDrawArrays(GL_LINES,0,(GLsizei)(cv.size()/3));
}

// ============================================================
//  drawRigidBodies
//
//  Each rigid body is drawn as a filled quad (two triangles)
//  using its 4 corner particles.  We apply a per-body colour
//  based on body index so boxes are visually distinct.
//  The static floor box gets a dark grey colour.
// ============================================================
void Renderer::drawRigidBodies(const Simulation& sim, const glm::mat4& VP)
{
    const auto& P  = sim.particles();
    const auto& RB = sim.rigidBodies();

    glUseProgram(m_faceShader);
    glUniformMatrix4fv(glGetUniformLocation(m_faceShader,"uVP"),1,GL_FALSE,glm::value_ptr(VP));
    glm::vec3 ld=glm::normalize(glm::vec3(0.4f,0.8f,0.6f));
    glUniform3fv(glGetUniformLocation(m_faceShader,"uLight"),1,glm::value_ptr(ld));

    // A small palette of gothic/dark colours for the boxes
    static const glm::vec4 palette[] = {
        {0.65f,0.20f,0.20f,1.f},  // dark red
        {0.20f,0.45f,0.65f,1.f},  // steel blue
        {0.55f,0.40f,0.15f,1.f},  // dark gold
        {0.30f,0.55f,0.30f,1.f},  // dark green
        {0.55f,0.25f,0.55f,1.f},  // purple
        {0.65f,0.45f,0.25f,1.f},  // bronze
    };
    static const int paletteSize = 6;

    for (int bi = 0; bi < (int)RB.size(); ++bi)
    {
        const RigidBody& rb = RB[bi];
        if (rb.particleIndices.size() < 4) continue;

        glm::vec4 tint = rb.isStatic
            ? glm::vec4(0.22f,0.22f,0.28f,1.f)
            : palette[bi % paletteSize];

        glUniform4fv(glGetUniformLocation(m_faceShader,"uTint"),1,glm::value_ptr(tint));

        // Grab the 4 corner positions
        int tl=rb.particleIndices[0], tr=rb.particleIndices[1];
        int br=rb.particleIndices[2], bl=rb.particleIndices[3];

        std::vector<float> verts = {
            P[tl].position.x, P[tl].position.y, P[tl].position.z,
            P[tr].position.x, P[tr].position.y, P[tr].position.z,
            P[br].position.x, P[br].position.y, P[br].position.z,
            P[bl].position.x, P[bl].position.y, P[bl].position.z,
        };
        std::vector<unsigned int> idx = {0,3,1, 1,3,2};
        uploadAndDrawTriangles(verts, idx);

        // Wireframe outline
        if (showWireframe)
        {
            glUseProgram(m_lineShader);
            glUniformMatrix4fv(glGetUniformLocation(m_lineShader,"uVP"),1,GL_FALSE,glm::value_ptr(VP));
            glUniform4f(glGetUniformLocation(m_lineShader,"uColor"),1.f,1.f,1.f,0.25f);
            std::vector<float> lv = {
                P[tl].position.x,P[tl].position.y,0.f, P[tr].position.x,P[tr].position.y,0.f,
                P[tr].position.x,P[tr].position.y,0.f, P[br].position.x,P[br].position.y,0.f,
                P[br].position.x,P[br].position.y,0.f, P[bl].position.x,P[bl].position.y,0.f,
                P[bl].position.x,P[bl].position.y,0.f, P[tl].position.x,P[tl].position.y,0.f,
            };
            uploadLines(lv);
            glDrawArrays(GL_LINES,0,(GLsizei)(lv.size()/3));
            glUseProgram(m_faceShader);
            glUniformMatrix4fv(glGetUniformLocation(m_faceShader,"uVP"),1,GL_FALSE,glm::value_ptr(VP));
        }
    }
}

void Renderer::drawClothMesh(const Simulation& sim, const glm::mat4& VP)
{
    const auto& P   = sim.particles();
    const auto& C   = sim.constraints();
    const auto& cfg = sim.config();
    const int rows=cfg.clothRows, cols=cfg.clothCols;
    const int MaxP=(int)P.size();

    // Adjacency for torn cloth
    std::vector<bool> live(MaxP*MaxP,false);
    for(const auto& c:C){int a=std::min(c.i,c.j),b=std::max(c.i,c.j);if(a>=0&&b<MaxP)live[a*MaxP+b]=true;}
    auto hasEdge=[&](int i,int j)->bool{int a=std::min(i,j),b=std::max(i,j);return a>=0&&b<MaxP&&live[a*MaxP+b];};
    auto pidx=[&](int r,int c){return r*cols+c;};

    std::vector<float> verts;
    verts.reserve(P.size()*3);
    for(const auto& p:P){verts.push_back(p.position.x);verts.push_back(p.position.y);verts.push_back(p.position.z);}

    std::vector<unsigned int> indices;
    for(int r=0;r<rows-1;++r)for(int c=0;c<cols-1;++c)
    {
        int tl=pidx(r,c),tr=pidx(r,c+1),bl=pidx(r+1,c),br=pidx(r+1,c+1);
        if(hasEdge(tl,tr)&&hasEdge(tl,bl)&&hasEdge(tr,br)&&hasEdge(bl,br))
        {indices.push_back(tl);indices.push_back(bl);indices.push_back(tr);
         indices.push_back(tr);indices.push_back(bl);indices.push_back(br);}
    }

    glUseProgram(m_faceShader);
    glUniformMatrix4fv(glGetUniformLocation(m_faceShader,"uVP"),1,GL_FALSE,glm::value_ptr(VP));
    glm::vec3 ld=glm::normalize(glm::vec3(0.4f,0.8f,0.6f));
    glUniform3fv(glGetUniformLocation(m_faceShader,"uLight"),1,glm::value_ptr(ld));
    glUniform4f(glGetUniformLocation(m_faceShader,"uTint"),0.22f,0.70f,0.65f,1.f);
    glDisable(GL_CULL_FACE);
    if(!indices.empty()) uploadAndDrawTriangles(verts,indices);
}

void Renderer::drawConstraintLines(const Simulation& sim, const glm::mat4& VP)
{
    const auto& P=sim.particles(); const auto& C=sim.constraints();
    if(C.empty()) return;
    glUseProgram(m_lineShader);
    glUniformMatrix4fv(glGetUniformLocation(m_lineShader,"uVP"),1,GL_FALSE,glm::value_ptr(VP));
    glUniform4f(glGetUniformLocation(m_lineShader,"uColor"),0.45f,0.75f,0.95f,0.85f);
    std::vector<float> ev;
    ev.reserve(C.size()*6);
    for(const auto& c:C){
        const auto& a=P[c.i].position;const auto& b=P[c.j].position;
        ev.push_back(a.x);ev.push_back(a.y);ev.push_back(a.z);
        ev.push_back(b.x);ev.push_back(b.y);ev.push_back(b.z);}
    uploadLines(ev);
    glDrawArrays(GL_LINES,0,(GLsizei)(ev.size()/3));
}

void Renderer::drawDragLine(const Simulation& sim, const glm::mat4& VP)
{
    const auto& mouse=sim.mouseCon();
    if(!mouse.active||mouse.particleIndex<0) return;
    glUseProgram(m_lineShader);
    glUniformMatrix4fv(glGetUniformLocation(m_lineShader,"uVP"),1,GL_FALSE,glm::value_ptr(VP));
    glUniform4f(glGetUniformLocation(m_lineShader,"uColor"),1.f,0.9f,0.15f,0.85f);
    const auto& pp=sim.particles()[mouse.particleIndex].position;
    float dv[]={pp.x,pp.y,pp.z,mouse.target.x,mouse.target.y,mouse.target.z};
    uploadLines({dv,dv+6});
    glDrawArrays(GL_LINES,0,2);
}

void Renderer::drawParticles(const Simulation& sim, const glm::mat4& VP)
{
    const auto& P=sim.particles(); const auto& mouse=sim.mouseCon();
    if(P.empty()) return;
    glUseProgram(m_ptShader);
    glUniformMatrix4fv(glGetUniformLocation(m_ptShader,"uVP"),1,GL_FALSE,glm::value_ptr(VP));
    float sz=sim.config().mode==3?7.f:14.f;
    glUniform1f(glGetUniformLocation(m_ptShader,"uSize"),sz);
    struct Pass{glm::vec4 col;bool pin;bool drag;};
    Pass passes[]={
        {{0.95f,0.52f,0.18f,1.f},false,false},
        {{0.75f,0.88f,1.00f,1.f},true, false},
        {{1.00f,0.95f,0.10f,1.f},false,true },
    };
    for(const auto& pass:passes)
    {
        glUniform4fv(glGetUniformLocation(m_ptShader,"uColor"),1,glm::value_ptr(pass.col));
        std::vector<float> pv;
        for(int i=0;i<(int)P.size();++i)
        {
            bool pin=P[i].invMass==0.f;
            bool drag=mouse.active&&mouse.particleIndex==i;
            if(pass.drag&&!drag) continue;
            if(!pass.drag&&drag) continue;
            if(!pass.drag&&pass.pin!=pin) continue;
            pv.push_back(P[i].position.x);pv.push_back(P[i].position.y);pv.push_back(P[i].position.z);
        }
        if(pv.empty()) continue;
        glBindVertexArray(m_ptVAO);
        glBindBuffer(GL_ARRAY_BUFFER,m_ptVBO);
        glBufferData(GL_ARRAY_BUFFER,pv.size()*sizeof(float),pv.data(),GL_DYNAMIC_DRAW);
        glDrawArrays(GL_POINTS,0,(GLsizei)(pv.size()/3));
    }
}

// ============================================================
//  Upload helpers
// ============================================================

void Renderer::uploadLines(const std::vector<float>& v)
{
    glBindVertexArray(m_lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_lineVBO);
    glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_DYNAMIC_DRAW);
}

void Renderer::uploadAndDrawTriangles(const std::vector<float>& verts,
                                       const std::vector<unsigned int>& idx)
{
    glBindVertexArray(m_faceVAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_faceVBO);
    glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(float),verts.data(),GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m_faceEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,idx.size()*sizeof(unsigned int),idx.data(),GL_DYNAMIC_DRAW);
    glDisable(GL_CULL_FACE);
    glDrawElements(GL_TRIANGLES,(GLsizei)idx.size(),GL_UNSIGNED_INT,0);
}

// ============================================================
//  Buffers & shaders
// ============================================================
void Renderer::buildBuffers()
{
    auto mk=[](GLuint& va,GLuint& vb){
        glGenVertexArrays(1,&va);glGenBuffers(1,&vb);
        glBindVertexArray(va);glBindBuffer(GL_ARRAY_BUFFER,vb);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
        glBindVertexArray(0);
    };
    mk(m_ptVAO,m_ptVBO); mk(m_lineVAO,m_lineVBO);
    glGenVertexArrays(1,&m_faceVAO);glGenBuffers(1,&m_faceVBO);glGenBuffers(1,&m_faceEBO);
    glBindVertexArray(m_faceVAO);
    glBindBuffer(GL_ARRAY_BUFFER,m_faceVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m_faceEBO);
    glBindVertexArray(0);
}
void Renderer::buildShaders()
{
    auto mk=[](const char* vs,const char* fs)->GLuint{
        auto cs=[](GLenum t,const char* s)->GLuint{
            GLuint id=glCreateShader(t);
            glShaderSource(id,1,&s,nullptr);glCompileShader(id);
            GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
            if(!ok){char l[512];glGetShaderInfoLog(id,512,nullptr,l);
                fprintf(stderr,"Shader:\n%s\n",l);throw std::runtime_error("shader");}
            return id;
        };
        GLuint v=cs(GL_VERTEX_SHADER,vs),f=cs(GL_FRAGMENT_SHADER,fs);
        GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);
        GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
        if(!ok){char l[512];glGetProgramInfoLog(p,512,nullptr,l);
            fprintf(stderr,"Link:\n%s\n",l);throw std::runtime_error("link");}
        glDeleteShader(v);glDeleteShader(f);return p;
    };
    m_ptShader  =mk(kPtVert,  kPtFrag);
    m_lineShader=mk(kLineVert,kLineFrag);
    m_faceShader=mk(kFaceVert,kFaceFrag);
}
GLuint Renderer::compileShader(GLenum t,const char* s)
{GLuint id=glCreateShader(t);glShaderSource(id,1,&s,nullptr);glCompileShader(id);return id;}
GLuint Renderer::linkProgram(GLuint v,GLuint f)
{GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);return p;}
