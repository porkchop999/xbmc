#version 100

precision highp float;
uniform sampler2D m_sampY;
uniform sampler2D m_sampU;
uniform sampler2D m_sampV;
varying vec2      m_cordY;
varying vec2      m_cordU;
varying vec2      m_cordV;
uniform float     m_alpha;
uniform mat4      m_yuvmat;
uniform float     m_stepX;
uniform float     m_stepY;
uniform int       m_field;

void main()
{
  vec4 yuv, rgb;

  vec2 offsetY;
  vec2 offsetU;
  vec2 offsetV;
  float temp1 = mod(m_cordY.y, 2.0*m_stepY);

  offsetY  = m_cordY;
  offsetU  = m_cordU;
  offsetV  = m_cordV;

  offsetY.y -= (temp1 - m_stepY/2.0 + float(m_field)*m_stepY);
  offsetU.y -= (temp1 - m_stepY/2.0 + float(m_field)*m_stepY)/2.0;
  offsetV.y -= (temp1 - m_stepY/2.0 + float(m_field)*m_stepY)/2.0;

  float bstep = step(m_stepY, temp1);

  // Blend missing line
  vec2 belowY, belowU, belowV;

  belowY.x = offsetY.x;
  belowY.y = offsetY.y + (2.0*m_stepY*bstep);
  belowU.x = offsetU.x;
  belowU.y = offsetU.y + (m_stepY*bstep);
  belowV.x = offsetV.x;
  belowV.y = offsetV.y + (m_stepY*bstep);

  vec4 yuvBelow, rgbBelow;

  yuv.rgba = vec4(texture2D(m_sampY, offsetY).r, texture2D(m_sampU, offsetU).g, texture2D(m_sampV, offsetV).a, 1.0);
  rgb   = m_yuvmat * yuv;
  rgb.a = m_alpha;

  yuvBelow.rgba = vec4(texture2D(m_sampY, belowY).r, texture2D(m_sampU, belowU).g, texture2D(m_sampV, belowV).a, 1.0);
  rgbBelow   = m_yuvmat * yuvBelow;
  rgbBelow.a = m_alpha;

  gl_FragColor.rgba = mix(rgb, rgbBelow, 0.5);
}