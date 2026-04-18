#pragma once
#include "GameState.h"
#include "Font.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>
#include <sstream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  HUD — proper text rendering using embedded bitmap font
// ============================================================
class HUD
{
public:
    HUD() = default;
    ~HUD()
    {
        glDeleteVertexArrays(1,&m_vao); glDeleteBuffers(1,&m_vbo);
        glDeleteBuffers(1,&m_ebo); glDeleteProgram(m_shader);
    }

    void init(int w, int h)
    {
        m_vpW=w; m_vpH=h;
        buildShader(); buildBuffers();
        m_font.init();
    }
    void resize(int w, int h){ m_vpW=w; m_vpH=h; }

    void draw(const GameState& gs, float bossTimer, float surviveDur,
              bool bossRage, float fadeAlpha=0.f)
    {
        float pw=(float)m_vpW, ph=(float)m_vpH;
        glm::mat4 proj=glm::ortho(0.f,pw,ph,0.f,-1.f,1.f);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

        // ---- Attempt counter (top left) ----
        {
            float fill=glm::min((float)gs.runThisLevel/20.f,1.f);
            float fr=glm::min(fill*2.f,1.f), fg=glm::max(1.f-fill*1.5f,0.f);
            drawRect(20.f,18.f,110.f,12.f, 0.07f,0.06f,0.09f,0.7f);
            drawRect(20.f,18.f,110.f*fill,12.f, fr*0.8f,fg*0.5f,0.2f,0.9f);
            // Text
            char buf[32]; snprintf(buf,sizeof(buf),"RUN %d",gs.runThisLevel+1);
            m_font.drawTextShadow(buf,22.f,20.f,2.f,1.f,0.85f,0.7f,0.9f,proj);
        }

        // ---- Level name (top center) ----
        {
            auto cfg=gs.currentConfig();
            std::string name = std::string(cfg.name);
            // Convert to uppercase for display
            for(auto& c:name) c=toupper(c);
            float tw=m_font.measureWidth(name,2.f);
            drawRect(pw*0.5f-tw*0.5f-8.f,12.f,tw+16.f,16.f,0.08f,0.06f,0.12f,0.75f);
            m_font.drawTextShadowCentered(name,pw*0.5f,15.f,2.f,
                                           0.7f,0.6f,1.0f,1.f,proj);
        }

        // ---- Score (top right) ----
        {
            char buf[32]; snprintf(buf,sizeof(buf),"%06d",gs.score);
            float tw=m_font.measureWidth(buf,2.f);
            drawRect(pw-tw-24.f,12.f,tw+16.f,16.f,0.08f,0.06f,0.12f,0.75f);
            m_font.drawTextShadow(buf,pw-tw-16.f,15.f,2.f,
                                   0.85f,0.75f,1.f,1.f,proj);
        }

        // ---- Boss timer bar ----
        if (gs.screen==GameScreen::BossFight && surviveDur>0.f)
        {
            float remaining=glm::max(surviveDur-bossTimer,0.f);
            float fill=remaining/surviveDur;
            float barW=320.f, barH=20.f;
            float bx=pw*0.5f-barW*0.5f, by=ph*0.13f;

            // Background
            drawRect(bx,by,barW,barH,0.06f,0.05f,0.10f,0.85f);
            // Fill
            float fr=bossRage?0.9f:0.3f, fg=bossRage?0.15f:0.75f, fb=bossRage?0.1f:0.9f;
            drawRect(bx,by,barW*fill,barH,fr,fg,fb,0.9f);
            // Border
            drawRectOutline(bx,by,barW,barH,0.55f,0.45f,0.8f,0.8f);

            // Tick marks at 15s intervals
            for(float t=15.f;t<surviveDur;t+=15.f)
                drawRect(bx+barW*(t/surviveDur)-0.5f,by,1.5f,barH,0.4f,0.35f,0.6f,0.4f);

            // Time remaining text
            char buf[32];
            int secs=(int)remaining;
            snprintf(buf,sizeof(buf),"%d",secs);
            m_font.drawTextShadowCentered(buf,pw*0.5f,by+3.f,2.5f,
                                           1.f,1.f,1.f,0.95f,proj);

            // RAGE text
            if (bossRage)
            {
                float pulse=0.7f+0.3f*sinf(m_time*8.f);
                m_font.drawTextShadowCentered("RAGE",pw*0.5f,by-20.f,3.f,
                                               1.f,0.3f*pulse,0.1f,pulse,proj);
            }
        }

        // ---- Rage screen flash ----
        if (bossRage)
        {
            float pulse=sinf(m_time*8.f)*0.5f+0.5f;
            drawRect(0.f,0.f,pw,ph,0.8f,0.1f,0.05f,0.06f*pulse);
        }

        // ---- Screen fade ----
        if (fadeAlpha>0.001f)
            drawRect(0.f,0.f,pw,ph,0.f,0.f,0.f,fadeAlpha);

        m_time+=0.016f;
    }

    void drawMainMenu(float pw, float ph)
    {
        glm::mat4 proj=glm::ortho(0.f,pw,ph,0.f,-1.f,1.f);
        drawRect(0.f,0.f,pw,ph,0.f,0.f,0.f,0.72f);
    }

    void drawGameOver(float pw, float ph, int score)
    {
        glm::mat4 proj=glm::ortho(0.f,pw,ph,0.f,-1.f,1.f);
        // Panel
        drawRect(pw*0.2f,ph*0.3f,pw*0.6f,ph*0.4f,0.06f,0.04f,0.08f,0.92f);
        drawRectOutline(pw*0.2f,ph*0.3f,pw*0.6f,ph*0.4f,0.55f,0.2f,0.2f,0.9f);
        m_font.drawTextShadowCentered("GAME OVER",pw*0.5f,ph*0.35f,4.f,
                                       0.9f,0.2f,0.2f,1.f,proj);
        char buf[64]; snprintf(buf,sizeof(buf),"SCORE  %d",score);
        m_font.drawTextShadowCentered(buf,pw*0.5f,ph*0.50f,2.f,
                                       0.8f,0.7f,1.f,0.9f,proj);
        m_font.drawTextShadowCentered("PRESS ENTER TO RETRY",
                                       pw*0.5f,ph*0.60f,2.f,
                                       0.6f,0.5f,0.8f,0.75f,proj);
    }

    void drawVictory(float pw, float ph, int score)
    {
        glm::mat4 proj=glm::ortho(0.f,pw,ph,0.f,-1.f,1.f);
        drawRect(pw*0.15f,ph*0.25f,pw*0.7f,ph*0.5f,0.05f,0.07f,0.12f,0.92f);
        drawRectOutline(pw*0.15f,ph*0.25f,pw*0.7f,ph*0.5f,0.3f,0.6f,0.8f,0.9f);
        m_font.drawTextShadowCentered("YOU SURVIVED",pw*0.5f,ph*0.30f,4.f,
                                       0.3f,0.8f,1.f,1.f,proj);
        m_font.drawTextShadowCentered("THE VOID",pw*0.5f,ph*0.40f,3.5f,
                                       0.5f,0.3f,0.9f,0.9f,proj);
        char buf[64]; snprintf(buf,sizeof(buf),"FINAL SCORE  %d",score);
        m_font.drawTextShadowCentered(buf,pw*0.5f,ph*0.55f,2.f,
                                       0.8f,0.7f,1.f,0.85f,proj);
    }

    void drawLevelComplete(float pw, float ph)
    {
        glm::mat4 proj=glm::ortho(0.f,pw,ph,0.f,-1.f,1.f);
        drawRect(pw*0.25f,ph*0.38f,pw*0.5f,ph*0.22f,0.07f,0.12f,0.09f,0.88f);
        drawRectOutline(pw*0.25f,ph*0.38f,pw*0.5f,ph*0.22f,0.2f,0.7f,0.3f,0.8f);
        m_font.drawTextShadowCentered("LEVEL CLEAR",pw*0.5f,ph*0.42f,3.f,
                                       0.2f,0.9f,0.4f,1.f,proj);
    }

    void drawBossDefeated(float pw, float ph, const char* bossName)
    {
        glm::mat4 proj=glm::ortho(0.f,pw,ph,0.f,-1.f,1.f);
        drawRect(pw*0.15f,ph*0.32f,pw*0.7f,ph*0.35f,0.08f,0.05f,0.12f,0.90f);
        drawRectOutline(pw*0.15f,ph*0.32f,pw*0.7f,ph*0.35f,0.6f,0.3f,0.8f,0.85f);
        m_font.drawTextShadowCentered("DEFEATED",pw*0.5f,ph*0.36f,3.f,
                                       0.7f,0.3f,1.f,1.f,proj);
        m_font.drawTextShadowCentered(bossName,pw*0.5f,ph*0.46f,2.f,
                                       1.f,0.7f,0.3f,0.9f,proj);
    }

    // Draw a text fragment popup near the player
    void drawFragmentPopup(const char* text, float px, float py, float alpha,
                           int vpW, int vpH)
    {
        if (alpha <= 0.f) return;
        glm::mat4 proj=glm::ortho(0.f,(float)vpW,(float)vpH,0.f,-1.f,1.f);
        float tw=m_font.measureWidth(text,2.f);
        drawRect(px-tw*0.5f-6.f,py-4.f,tw+12.f,16.f,0.1f,0.08f,0.15f,0.8f*alpha);
        m_font.drawTextShadowCentered(text,px,py,2.f,
                                       0.85f,0.78f,0.55f,alpha,proj);
    }

private:
    BitmapFont m_font;

    void drawRect(float x,float y,float w,float h,
                  float r,float g,float b,float a)
    {
        std::vector<float> v={x,y,0.f, x+w,y,0.f, x+w,y+h,0.f, x,y+h,0.f};
        std::vector<unsigned int> idx={0,1,2,0,2,3};
        glUseProgram(m_shader);
        glm::mat4 proj=glm::ortho(0.f,(float)m_vpW,(float)m_vpH,0.f,-1.f,1.f);
        glUniformMatrix4fv(glGetUniformLocation(m_shader,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
        glUniform4f(glGetUniformLocation(m_shader,"uColor"),r,g,b,a);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER,m_vbo);
        glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,idx.size()*sizeof(unsigned int),idx.data(),GL_DYNAMIC_DRAW);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
        glBindVertexArray(0);
    }

    void drawRectOutline(float x,float y,float w,float h,
                         float r,float g,float b,float a)
    {
        float t=1.5f;
        drawRect(x,y,w,t,r,g,b,a); drawRect(x,y+h-t,w,t,r,g,b,a);
        drawRect(x,y,t,h,r,g,b,a); drawRect(x+w-t,y,t,h,r,g,b,a);
    }

    void buildShader()
    {
        const char* vs=R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uProj;
void main(){ gl_Position=uProj*vec4(aPos,1.0); }
)glsl";
        const char* fs=R"glsl(
#version 330 core
out vec4 O; uniform vec4 uColor;
void main(){ O=uColor; }
)glsl";
        auto cs=[](GLenum t,const char* s)->GLuint{
            GLuint id=glCreateShader(t);
            glShaderSource(id,1,&s,nullptr);glCompileShader(id);return id;
        };
        GLuint v=cs(GL_VERTEX_SHADER,vs),f=cs(GL_FRAGMENT_SHADER,fs);
        m_shader=glCreateProgram();
        glAttachShader(m_shader,v);glAttachShader(m_shader,f);glLinkProgram(m_shader);
        glDeleteShader(v);glDeleteShader(f);
    }

    void buildBuffers()
    {
        glGenVertexArrays(1,&m_vao);glGenBuffers(1,&m_vbo);glGenBuffers(1,&m_ebo);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER,m_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m_ebo);
        glBindVertexArray(0);
    }

    GLuint m_shader=0,m_vao=0,m_vbo=0,m_ebo=0;
    int m_vpW=1200,m_vpH=850;
    float m_time=0.f;
};
