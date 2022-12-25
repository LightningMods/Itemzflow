/*
    shader_common.c

    GLES2 shaders facilities about loading and building shaders

    2019-21, masterzorag
*/

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <unistd.h> // sleep
#include "defines.h"

GLuint create_vbo(const GLsizeiptr size, const GLvoid* data, const GLenum usage)
{
    assert(data != NULL);
    GLuint vbo_object;
    glGenBuffers(1, &vbo_object);
    assert(vbo_object != 0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_object);
    glBufferData(GL_ARRAY_BUFFER, size, data, usage);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return vbo_object;
}

static GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader)
{
    GLuint programHandle = glCreateProgram();
    glAttachShader(programHandle, vertexShader);
    glAttachShader(programHandle, fragmentShader);
    glLinkProgram(programHandle);

    GLint linkSuccess;
    glGetProgramiv(programHandle, GL_LINK_STATUS, &linkSuccess);
    if (linkSuccess == GL_FALSE)
    {
        log_error("GL_LINK_STATUS error!"); programHandle = 0;
    }

    /* once we have the SL porgram, don't leak any shader! */
    if (vertexShader) { glDeleteShader(vertexShader), vertexShader = 0; }
    if (fragmentShader) { glDeleteShader(fragmentShader), fragmentShader = 0; }

    return programHandle;
}

static GLuint compile(GLenum type, const char* source, int size) {

    GLint compile_result;
    GLchar msg[512];

    GLuint handle = glCreateShader(type);
    if (!handle) {
        log_error("GLShader::compile: glCreateShader failed (%s) %i", type == GL_VERTEX_SHADER ? "vertex" : "fragment", glGetError());
        return GL_FALSE;
    }
    if (size > 0) {
        log_info("compile: glShaderBinary: type: %s, handle = %i, size = %i", type == GL_VERTEX_SHADER ? "vertex" : "fragment", handle, size);
        glShaderBinary(1, &handle, 0, (const GLvoid*)source, size);
    }
    else {
        log_info("compile: glShaderSource: type: %s, handle = %i, size = %i", type == GL_VERTEX_SHADER ? "vertex" : "fragment", handle, strlen(source));
        glShaderSource(handle, 1, &source, 0);
        glCompileShader(handle);
    }

    glGetShaderiv(handle, GL_COMPILE_STATUS, &compile_result);
    if (compile_result == GL_FALSE) {
        glGetShaderInfoLog(handle, sizeof(msg), NULL, msg);
        log_info("GLShader::compile: %u: %s\n", type, msg);
        glDeleteShader(handle);
        return GL_FALSE;
    }

    return handle;
}


GLuint BuildProgram(const char* vShader, const char* fShader, int vs_size, int fs_size)
{
    return LinkProgram(compile(GL_VERTEX_SHADER, vShader, vs_size), compile(GL_FRAGMENT_SHADER, fShader, fs_size));
}
