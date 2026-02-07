foo = []
foo += [('KeyPressMask', (1<<0))]
foo += [('KeyReleaseMask', (1<<1))]
foo += [('ButtonPressMask', (1<<2))]
foo += [('ButtonReleaseMask', (1<<3))]
foo += [('EnterWindowMask', (1<<4))]
foo += [('LeaveWindowMask', (1<<5))]
foo += [('PointerMotionMask', (1<<6))]
foo += [('PointerMotionHintMask', (1<<7))]
foo += [('Button1MotionMask', (1<<8))]
foo += [('Button2MotionMask', (1<<9))]
foo += [('Button3MotionMask', (1<<10))]
foo += [('Button4MotionMask', (1<<11))]
foo += [('Button5MotionMask', (1<<12))]
foo += [('ButtonMotionMask', (1<<13))]
foo += [('KeymapStateMask', (1<<14))]
foo += [('ExposureMask', (1<<15))]
foo += [('VisibilityChangeMask', (1<<16))]
foo += [('StructureNotifyMask', (1<<17))]
foo += [('ResizeRedirectMask', (1<<18))]
foo += [('SubstructureNotifyMask', (1<<19))]
foo += [('SubstructureRedirectMask', (1<<20))]
foo += [('FocusChangeMask', (1<<21))]
foo += [('PropertyChangeMask', (1<<22))]
foo += [('ColormapChangeMask', (1<<23))]
foo += [('OwnerGrabButtonMask', (1<<24))]

#event_mask = 0x0043807c
event_mask = 0x0062c07f

for name, mask in foo:
	if event_mask & mask:
		print(f'{hex(mask)}: {name}')
