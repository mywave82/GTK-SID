#include "resid/sid.h"
#include "sid-instance.h"

reSID::SID *instance;
int samplerate = 44100;
reSID::cycle_count cc;

extern "C" void sid_instance_init(void)
{
	instance = new reSID::SID();
	instance->set_sampling_parameters(985248, reSID::sampling_method::SAMPLE_INTERPOLATE, samplerate);
}

extern "C" void sid_instance_done(void)
{
	if (instance)
	{
		delete instance;
		instance = 0;
	}
}


extern "C" int sid_instance_clock(int16_t *buf, int buflen)
{
//	cc += 985248 / 25;
	cc = 985248; /* clock is always 1 second into the future */
	return instance->clock (cc, buf, buflen);
}

extern "C" void sid_instance_write (uint_least8_t addr, uint8_t data)
{
	instance->write(addr, data);
}
