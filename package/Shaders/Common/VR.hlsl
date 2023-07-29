/**
 Converts uv to the eye specific uv.

In VR, texture buffers include the lef tand right eye in the same buffer. 
This means the x value [0, .5] represents the left eye, and the y value (.5, 1] are the right eye.

This returns the adjusted value

@param uv - uv coords [0,1]
@param a_eyeIndex The eyeIndex; 0 is left, 1 is right
@returns adjused uv coords for the VR texture buffer
*/
float2 ConvertToVRUV(float2 uv)
{
#ifdef VR
	if (uv.x >= .5)
		uv.x = (uv.x / 2 + .5);  // Right Eye: [0, 1] -> [.5,1];
	else
		uv.x = uv.x / 2;  //	Left Eye: [0, 1] -> [0,.5],
#endif
	return uv;
}
