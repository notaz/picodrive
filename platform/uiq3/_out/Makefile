all: PicoDrive.SIS

PicoDrive.SIS: ..\PicoDrive.exe ..\rsc\PicoDrive.rsc ..\rsc\PicoDrive.mbm ..\rsc\PicoDrive_loc.rsc ..\rsc\PicoDrive_reg.rsc
	makesis PicoDrive.pkg

clean:
	@erase 2>>nul PicoDrive.pkg
