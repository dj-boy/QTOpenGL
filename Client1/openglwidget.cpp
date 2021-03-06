﻿#include <openglwidget.h>
#include <QOpenGLShaderProgram>
#include <QCoreApplication>
#include <QDateTime>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QIODevice>
#include <QVector>
#include <QVector3D>
#include <QDebug>
#include <QTransform>
#include <QtCore/QtMath>

const float PI = 3.1415926f;


GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      QOpenGLFunctions_4_5_Core(),
      m_vbo(QOpenGLBuffer::VertexBuffer),
      m_colorvbo(QOpenGLBuffer::VertexBuffer),
      m_normal(QOpenGLBuffer::VertexBuffer)
{
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(4, 5);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    setFocusPolicy(Qt::StrongFocus);

    count = 0;
    mode = PointCloud;//!默认点云渲染

    x0 = 0.0f;
    x1 = 100.0f;
    y0 = 0.0f;
    y1 = 100.0f;
    z0 = 0.0f;
    z1 = 1000.0f;
    anglex = -70.0f;
    angley = 0.0f;
    anglez = -135.0f;
    PosX = 0.0f;    //! x位置
    PosY = 0.0f;    //! y位置
    PosZ = -500.0f; //! z位置
    scal = 1.0f;    //! 缩放因子

    material.ambient = QVector3D(0.02f, 0.02f, 0.02f);
    material.diffuse = QVector3D(0.01f, 0.01f, 0.01f);
    material.specular = QVector3D(0.5f, 0.5f, 0.5f);
    material.shinines = 16.0f;

    light.ambient = QVector3D(0.5f, 0.5f, 0.5f);
    light.diffuse = QVector3D(0.2f, 0.2f, 0.2f);
    light.specular = QVector3D(0.8f, 0.8f, 0.8f);
    light.postion = QVector3D(25.0f, 150.0f, 100.0f);//! 相机坐标系下的光源位置
}

GLWidget::~GLWidget()
{
}

QSize GLWidget::minimumSizeHint() const
{
    return QSize(50, 50);
}

QSize GLWidget::sizeHint() const
{
    return QSize(1280, 760);
}

void GLWidget::initShaders()
{
    //! 模型着色器
    m_program = new QOpenGLShaderProgram;
    m_program->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/Dvs.shader");
    m_program->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/Dfs.shader");
    if (!m_program->link())
        close();
    if (!m_program->bind())
        close();
//    glUniform1i(m_program->uniformLocation("shadowMap"), 0);
     m_program->release();

    //! 坐标轴着色器
    coordPro = new QOpenGLShaderProgram;
    coordPro->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/coordvs.vert");
    coordPro->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/coordfs.frag");
    if (!coordPro->link())
        close();

    if (!coordPro->bind())
        close();
    coordPro->release();

//    //! 阴影着色器
//    shadow_prog = new QOpenGLShaderProgram;
//    shadow_prog->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/depth.vert");
//    shadow_prog->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/depth.frag");
//    if (!shadow_prog->link())
//        close();

//    if (!shadow_prog->bind())
//        close();
//    shadow_prog->release();

}

void GLWidget::updateData(QVector<QVector3D> local, QVector<QVector3D> normal, QVector<QVector3D> color)
{
    if (local.empty())
    {
//        qDebug() << "OpenGL未接收到数据\n";
//        return;
    }
    m_vao.bind();
    m_vbo.bind();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    count = 0;
    m_vbo.allocate(2 * local.size() * 3 * static_cast<int>(sizeof(float)));
    m_vbo.write(0, local.data(), local.size() * 3 * static_cast<int>(sizeof(float)));
    m_vbo.write(local.size() * 3 * static_cast<int>(sizeof(float)), normal.data(),
                normal.size() * 3 * static_cast<int>(sizeof(float)));
    count = local.size();
    update();
}

void GLWidget::cleanup()
{
    if (m_program == nullptr)
        return;
    makeCurrent();
    delete m_program;
    m_program = nullptr;
    doneCurrent();
}

void GLWidget::initializeGL()
{
    initializeOpenGLFunctions();//创建OpenGL上下文环境，初始化OpenGL
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &GLWidget::cleanup);//最好有这句话
//    const GLfloat color[] = { 0.2f, 0.3f, 0.3f, 1.0f };
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
//    glClearBufferfv(GL_COLOR, 0, color);
    initShaders();

    //!创建uniform缓冲区
    glGenBuffers(1, &uboExampleBlock);
    glBindBuffer(GL_UNIFORM_BUFFER, uboExampleBlock);
    glBufferData(GL_UNIFORM_BUFFER, 2 * 16 * sizeof(float), nullptr, GL_STATIC_DRAW);//分配uniform块空间
    //!ubo缓冲区绑定点
    uboBindPoint = 1;
    glBindBufferRange(GL_UNIFORM_BUFFER, uboBindPoint, uboExampleBlock, 0, 2 * 16 * sizeof(float));//!缓冲区绑定点
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //!着色器绑定uniform块
    //! 模型着色器
    m_program->bind();
    unsigned int mprogramuniformID = glGetUniformBlockIndex(m_program->programId(), "Matrices");
    glUniformBlockBinding(m_program->programId(), mprogramuniformID, uboBindPoint);
    m_program->release();
    //!坐标轴着色器
    coordPro->bind();
    unsigned int coordProuniformID = glGetUniformBlockIndex(coordPro->programId(), "Matrices");
    glUniformBlockBinding(coordPro->programId(), coordProuniformID, uboBindPoint);
    coordPro->release();


    //! 创建缓冲区
    m_vao.create();
    m_vbo.create();
    m_colorvbo.create();
    m_normal.create();

//    glGenFramebuffers(1, &fbo);
//    glGenTextures(1, &depth_texture);
//    glBindTexture(GL_TEXTURE_2D, depth_texture);
//    // 设置纹理缓冲参数
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
//    GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
//    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

//    // 纹理缓冲绑定到帧缓冲
//    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
//    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);
//    glDrawBuffer(GL_NONE);
//    glReadBuffer(GL_NONE);
//    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //! 坐标轴数据
    QVector<QVector3D> out;
    genCoordData(QVector3D(400, 100, 100), QVector3D(50, 30, 30), out);
    indexSize = out.size();
    VBO.create();
    VBO.bind();
    VBO.allocate(out.data(), out.size() * 3 * static_cast<int>(sizeof(float)));//!必须先绑定才能赋值
    VAO.create();

    m_camera.setToIdentity();
    m_camera.translate(PosX, PosY, PosZ);
    m_camera.rotate(anglex, QVector3D(1.0f, 0.0f, 0.0f));
    m_camera.rotate(angley, QVector3D(0.0f, 1.0f, 0.0f));
    m_camera.rotate(anglez, QVector3D(0.0f, 0.0f, 1.0f));
    m_model.setToIdentity();

}


void GLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);//允许深度测试
//    glEnable(GL_LINE_SMOOTH);
//    glEnable(GL_PROGRAM_POINT_SIZE);

    //! 开始渲染
    QPainter painter(this);
    painter.beginNativePainting();
    //! 画坐标轴
    drawCoord();
    //!画煤场
//    drawShadow();
    drawCoal();

    //! 文本
    drawText(painter);
    painter.endNativePainting();
}

void GLWidget::resizeGL(int w, int h)
{
    //! 初始默认坐标轴在窗口中间
    //! w, h是像素值
    m_proj.setToIdentity();
//    m_proj.perspective(45, 1.0f *  w / h, 0.1f, 1000.0f);
    m_proj.ortho(-w / 2.0f, w / 2.0f, -h / 2.0f, h / 2.0f, 0.1f, 1000.0f);

    glBindBuffer(GL_UNIFORM_BUFFER, uboExampleBlock);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 16 * sizeof(float), m_proj.data());
    glBufferSubData(GL_UNIFORM_BUFFER, 16 * sizeof(float),
                    16 * sizeof(float), m_model.data());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    update();
}

void GLWidget::paintOpenGL()
{
    update();
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
   m_lastPos = event->localPos();
}

void GLWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        anglex = -70.0f;
        angley = 0.0f;
        anglez = -135.0f;
        PosX = 0.0f;    //! x位置
        PosY = 0.0f;    //! y位置
        PosZ = -500.0f; //! z位置
        scal = 1.0f;
        m_camera.setToIdentity();
        m_camera.translate(PosX, PosY, PosZ);
        m_camera.rotate(anglex, QVector3D(1.0f, 0.0f, 0.0f));
        m_camera.rotate(angley, QVector3D(0.0f, 1.0f, 0.0f));
        m_camera.rotate(anglez, QVector3D(0.0f, 0.0f, 1.0f));
        m_proj.setToIdentity();
        m_proj.ortho(-this->width() / 2.0f, this->width() / 2.0f,
                     -this->height() / 2.0f, this->height() / 2.0f, 0.1f, 1000.0f);
        m_model.setToIdentity();
        update();
    }
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    QMatrix4x4 transform;
    transform.setToIdentity();
    QPointF currentPoint = event->localPos();
    float dx = static_cast<float>(currentPoint.x() - m_lastPos.x());//移动的像素距离
    float dy = static_cast<float>(currentPoint.y() - m_lastPos.y());

    if (event->buttons() & Qt::LeftButton)
    {
        anglex += dy;
        anglez += dx;
    }
    if (event->buttons() & Qt::RightButton)
    {
        PosX += dx;
        PosY -= dy;
    }
    float cz = cosf(anglez / 180.0f * PI);
    float sz = sinf(anglez / 180.0f * PI);
    float cx = cosf(anglex / 180.0f * PI);
    float sx = sinf(anglex / 180.0f * PI);
    float cy = cosf(angley / 180.0f * PI);
    float sy = sinf(angley / 180.0f * PI);

    //! 旋转顺序：z-y-x
    m_camera.setRow(0, QVector4D(cz*cy, -cy*sz, sy, PosX));
    m_camera.setRow(1, QVector4D(sx*sy*cz+cx*sz, -sx*sy*sz+cx*cz, -sx*cy, PosY));
    m_camera.setRow(2, QVector4D(-cx*sy*cz+sx*sz, cx*sy*sz+sx*cz, cx*cy, PosZ));

    m_camera *= QMatrix4x4(scal, 0.0, 0.0f, 0.0f,
                           0.0f, scal, 0.0f, 0.0f,
                           0.0f, 0.0f, scal, 0.0f,
                           0.0f, 0.0f, 0.0f, 1.0f);

    m_lastPos = currentPoint;
    update();
}


void GLWidget::wheelEvent(QWheelEvent *event)
{
    QMatrix4x4 s;
    s.setToIdentity();
    if (event->delta() > 0)//放大
    {
        scal *= 1.5f;
        s *= QMatrix4x4(1.5f, 0.0, 0.0f, 0.0f,
                           0.0f, 1.5f, 0.0f, 0.0f,
                           0.0f, 0.0f, 1.5f, 0.0f,
                           0.0f, 0.0f, 0.0f, 1.0f);

    }
    else//缩小
    {
        scal *= 0.8f;
        s *= QMatrix4x4(0.8f, 0.0, 0.0f, 0.0f,
                           0.0f, 0.8f, 0.0f, 0.0f,
                           0.0f, 0.0f, 0.8f, 0.0f,
                           0.0f, 0.0f, 0.0f, 1.0f);

    }
    if (scal < static_cast<float>(qPow(0.8, 5)))
    {
        scal = static_cast<float>(qPow(0.8, 5));
    }
   else if (scal > static_cast<float>(qPow(1.5, 5)))
    {
        scal = static_cast<float>(qPow(1.5, 5));
    }
    else
    {
        m_camera *= s;

    }
    update();
}


void GLWidget::setRenderMode(RenderMode mode)
{
    this->mode = mode;
}

bool GLWidget::genCoordData(const QVector3D max, const QVector3D step,
                  QVector<QVector3D>& out)
{
    float x = max.x();
    float y = max.y();
    float z = max.z();

    float stepx = step.x();
    float stepy = step.y();
    float stepz = step.z();

    if ((abs(x - 0.0f) < 1e-3f || abs(y - 0.0f) < 1e-3f || abs(z - 0.0f) < 1e-3f ||
            abs(stepx - 0.0f) < 1e-3f || abs(stepy - 0.0f) < 1e-3f || abs(stepz - 0.0f) < 1e-3f))
    {
        return false;
    }

    int numX = static_cast<int>(x / stepx);
    int numY = static_cast<int>(y / stepy);
    int numZ = static_cast<int>(z / stepz);
    //!生成刻度
    //! x方向刻度
    for (int i = 0; i < numX; ++i)
    {
        out.push_back(QVector3D((i + 1) * stepx, 0.0f, 0.0f));//!刻度起点
        out.push_back(QVector3D((i + 1) * stepx, 0.0f, -2.0f));//!刻度终点
        QString label = QString::number(static_cast<int>((i + 1) * stepx));
        labels[label].push_back(QVector3D((i + 1) * stepx, 0.0f, -2.0f));
    }
    //! y方向刻度
    for (int i = 0; i < numY; ++i)
    {
        out.push_back(QVector3D(0.0f, (i + 1) * stepy, 0.0f));//!刻度起点
        out.push_back(QVector3D(-2.0f, (i + 1) * stepy, -2.0f));//!刻度终点
        QString label = QString::number(static_cast<int>((i + 1) * stepy));
        labels[label].push_back(QVector3D(-2.0f, (i + 1) * stepy, -2.0f));
    }
    //! z方向刻度
    for (int i = 0; i < numZ; ++i)
    {
        out.push_back(QVector3D(0.0f, 0.0f, (i + 1) * stepz));//!刻度起点
        out.push_back(QVector3D(0.0f,-2.0f, (i + 1) * stepz));//!刻度终点
        QString label = QString::number(static_cast<int>((i + 1) * stepz));
        labels[label].push_back(QVector3D(0.0f,-2.0f, (i + 1) * stepz));
    }
    //! xyz轴
    out.push_back(QVector3D(0.0f, 0.0f, 0.0f));
    out.push_back(QVector3D(1.0f * x, 0.0f, 0.0f));

    out.push_back(QVector3D(0.0f, 0.0f, 0.0f));
    out.push_back(QVector3D(0.0f, 1.0f * y, 0.0f));

    out.push_back(QVector3D(0.0f, 0.0f, 0.0f));
    out.push_back(QVector3D(0.0f, 0.0f, 1.0f * z));

    //! xyz标签
    QString label("X");
    labels[label].push_back(QVector3D(1.0f * x + 10.0f, 0.0f, 0.0f));
    label = "Y";
    labels[label].push_back(QVector3D(0.0f, 1.0f * y + 10.0f, 0.0f));
    label = "Z";
    labels[label].push_back(QVector3D(0.0f, 0.0f, 1.0f * z + 10.0f));
    return true;
}

void GLWidget::rotateRight()
{
    anglez += 10;
    float cz = cosf(anglez / 180.0f * PI);
    float sz = sinf(anglez / 180.0f * PI);
    float cx = cosf(anglex / 180.0f * PI);
    float sx = sinf(anglex / 180.0f * PI);
    float cy = cosf(angley / 180.0f * PI);
    float sy = sinf(angley / 180.0f * PI);
    m_camera.setRow(0, QVector4D(cz*cy, -cy*sz, sy, PosX));
    m_camera.setRow(1, QVector4D(sx*sy*cz+cx*sz, -sx*sy*sz+cx*cz, -sx*cy, PosY));
    m_camera.setRow(2, QVector4D(-cx*sy*cz+sx*sz, cx*sy*sz+sx*cz, cx*cy, PosZ));
    m_camera *= QMatrix4x4(scal, 0.0, 0.0f, 0.0f,
                           0.0f, scal, 0.0f, 0.0f,
                           0.0f, 0.0f, scal, 0.0f,
                           0.0f, 0.0f, 0.0f, 1.0f);
    update();
}
void GLWidget::rotateLeft()
{
    anglez -= 10;
    float cz = cosf(anglez / 180.0f * PI);
    float sz = sinf(anglez / 180.0f * PI);
    float cx = cosf(anglex / 180.0f * PI);
    float sx = sinf(anglex / 180.0f * PI);
    float cy = cosf(angley / 180.0f * PI);
    float sy = sinf(angley / 180.0f * PI);
    m_camera.setRow(0, QVector4D(cz*cy, -cy*sz, sy, PosX));
    m_camera.setRow(1, QVector4D(sx*sy*cz+cx*sz, -sx*sy*sz+cx*cz, -sx*cy, PosY));
    m_camera.setRow(2, QVector4D(-cx*sy*cz+sx*sz, cx*sy*sz+sx*cz, cx*cy, PosZ));
    m_camera *= QMatrix4x4(scal, 0.0, 0.0f, 0.0f,
                           0.0f, scal, 0.0f, 0.0f,
                           0.0f, 0.0f, scal, 0.0f,
                           0.0f, 0.0f, 0.0f, 1.0f);
    update();
}

void GLWidget::drawCoal()
{
//    glViewport(0, 0, this->width(), this->height());
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_program->bind();
    glUniformMatrix4fv(m_program->uniformLocation("camera"),
                       1, GL_FALSE, m_camera.data());

    glUniform3fv(m_program->uniformLocation("material.ambient"), 1, &(material.ambient[0]));
    glUniform3fv(m_program->uniformLocation("material.diffuse"), 1, &(material.diffuse[0]));
    glUniform3fv(m_program->uniformLocation("material.specular"), 1, &(material.specular[0]));
    glUniform1f(m_program->uniformLocation("material.shininess"), material.shinines);

    QVector3D lightDirection = (m_camera * QVector4D(light.postion, 1.0f)).toVector3D();
    glUniform3fv(m_program->uniformLocation("light.ambient"), 1, &(light.ambient[0]));
    glUniform3fv(m_program->uniformLocation("light.diffuse"), 1, &(light.diffuse[0]));
    glUniform3fv(m_program->uniformLocation("light.specular"), 1, &(light.specular[0]));
    glUniform3fv(m_program->uniformLocation("light.position"), 1, &(lightDirection[0]));
//    glUniform3fv(m_program->uniformLocation("light.direction"), 1, &(lightDirection[0]));
    m_vao.bind();
    m_vbo.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, false, 3 * sizeof(float), (void*)(count * 3 * sizeof(float)));
    if (Patch == mode)
    {
//        glPolygonMode(GL_FRONT, GL_FILL);//!这种方式有一点缺陷--会改变所有三角形图元的渲染模式，不管三角形在此之前已被渲染还是在此之后渲染
        glDrawArrays(GL_TRIANGLES, 0, count);
    }
    else if (PointCloud == mode)
    {
//        glPolygonMode(GL_FRONT_FACE, GL_POINT);
        glDrawArrays(GL_POINTS, 0, count);
    }
    else if(Lines == mode)
    {
//        glPolygonMode(GL_FRONT, GL_LINES);
        glDrawArrays(GL_LINES, 0, count);
    }
    else
    {}
    m_vao.release();
    m_vbo.release();
    m_program->release();
}

void GLWidget::drawShadow()
{

//    shadow_prog->bind();
//    //! 光源坐标系下的视图矩阵
//    int w = this->width();
//    int h = this->height();
//    QMatrix4x4 lightProjection, lightView;
//    lightProjection.setToIdentity();
//    lightProjection.ortho(-w / 2.0f, w / 2.0f, -h / 2.0f, h / 2.0f, 0.1f, 1000.0f);
//    lightView.setToIdentity();
//    lightView.lookAt(light.postion, QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
//    QMatrix4x4 lightSpaceMatrix = lightProjection * lightView;
//    glUniformMatrix4fv(shadow_prog->uniformLocation("lightSpaceMatrix"),
//                 1, GL_FALSE, lightSpaceMatrix.data());
//    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
//    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
//    glClear(GL_DEPTH_BUFFER_BIT);
//    m_vao.bind();
//    m_vbo.bind();
//    glEnableVertexAttribArray(0);
//    glVertexAttribPointer(0, 3, GL_FLOAT, false, 3 * sizeof(float), nullptr);
//    glEnableVertexAttribArray(1);
//    glVertexAttribPointer(1, 3, GL_FLOAT, false, 3 * sizeof(float), (void*)(count * 3 * sizeof(float)));
//    if (Patch == mode)
//    {
////        glPolygonMode(GL_FRONT, GL_FILL);//!这种方式有一点缺陷--会改变所有三角形图元的渲染模式，不管三角形在此之前已被渲染还是在此之后渲染
//        glDrawArrays(GL_TRIANGLES, 0, count);
//    }
//    else if (PointCloud == mode)
//    {
////        glPolygonMode(GL_FRONT_FACE, GL_POINT);
//        glDrawArrays(GL_POINTS, 0, count);
//    }
//    else if(Lines == mode)
//    {
////        glPolygonMode(GL_FRONT, GL_LINES);
//        glDrawArrays(GL_LINES, 0, count);
//    }
//    else
//    {}
//    m_vao.release();
//    m_vbo.release();
//    m_program->release();
//    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLWidget::drawCoord()
{
    //! 画坐标轴
    coordPro->bind();
    int modelLoc = coordPro->uniformLocation("camera");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, m_camera.data());
    VAO.bind();
    VBO.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, indexSize);
    VBO.release();
    VAO.release();
    coordPro->release();
}

void GLWidget::drawText(QPainter& pt)
{
    QPen pen;
    pen.setColor(Qt::red);
    pt.setPen(pen);
    for (QHash<QString, QList<QVector3D>>::iterator it = labels.begin(); it != labels.end(); ++it)
    {
        QList<QVector3D> points = it.value();
        for (int i = 0; i < points.size(); ++i)
        {
            QVector3D d3 = points.at(i);
            QVector4D ndc = m_proj * m_camera * m_model * QVector4D(d3, 1.0f);
//            QVector4D cam = m_camera * m_model * QVector4D(d3, 1.0f);
//            ndc = ndc / cam[2];
            //!计算像素
            int x = static_cast<int>((this->width() * 0.5f - 0.5f) * (ndc[0] + ndc[3]));
            int y = static_cast<int>((this->height() * 0.5f - 0.5f) * (ndc[3] - ndc[1]));
            pt.drawText(x, y, it.key());
        }
    }
    //! 画出光源
    QVector4D ndc = m_proj * m_camera * QVector4D(light.postion, 1.0f);
    int x = static_cast<int>((this->width() * 0.5f - 0.5f) * (ndc[0] + ndc[3]));
    int y = static_cast<int>((this->height() * 0.5f - 0.5f) * (ndc[3] - ndc[1]));
    pt.drawText(x, y, "O");
}
