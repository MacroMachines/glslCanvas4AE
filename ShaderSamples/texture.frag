uniform vec2 u_resolution;
uniform vec2 u_mouse;
uniform float u_time;
uniform sampler2D u_tex0;

void main(){
	
	vec2 st = gl_FragCoord.xy / u_resolution;
	
	vec3 color = texture2D(u_tex0, st).bgr;
	
	gl_FragColor = vec4(color, 1.0);
}
