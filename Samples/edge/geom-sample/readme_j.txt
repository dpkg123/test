[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.


�l�X�ȊȒP�ȃV�[����`�悵�� Edge Geometry ���C�u�����̋@�\���Љ�
����T���v��


<�T���v���v���O�����̃|�C���g>
  �{�T���v���́Aedgegeomcompiler �c�[���ɂ���ď������ꂽ�V�[����ǂݍ���
�@���@����ѕ`�悷����@���������܂��B�X�L�j���O�A�u�����h�V�F�C�v�A�O�p
�@�`�̃J�����O�A�J�X�^�����_���k�����ăX�^�e�B�b�N�ȃV�[���̍œK���ȂǁA
�@Edge Geometry �̋@�\���g�p����Ă���l�X�ȊȒP�ȃe�X�g�V�[����񋟂���
�@���B

<�T���v���v���O�������>
  edgegeomcompiler �̓V�[������������ہA�o�C�i���� .edge �t�@�C���Ɠǂ�
�@�Ղ� C �w�b�_�t�@�C����2�̈قȂ�t�H�[�}�b�g�ɏo�͂��������݂܂��B
�@�w�b�_�t�@�C���́A���l�� Edge Geometry �\���̂��ǂ̂悤�ɂ��Č݂��ɑg��
  ���킳��Ă��邩�m�F����ۂɖ𗧂��܂��B�܂��A�f�[�^�̐��������m�F����
�@���Ƃ��ł��܂��B
  ���̃T���v���͗����̃^�C�v�̃V�[����ǂݍ��ނ��Ƃ��ł��܂��B�f�t�H���g
�@�ł́A���̃T���v���̓o�C�i���V�[���t�@�C����ǂݍ��݂܂��i�R�}���h���C
�@���̈����Ƃ��ēn���j�B�w�b�_�V�[���t�@�C����ǂݍ��ނɂ́Amain.cpp �ɂ�
�@�� USE_SCENE_IN_HEADE �̒�`���R�����g����O���A�w�b�_�[�t�@�C���ւ̃p
  �X�𒼐ڎw�肵�Ă��������B

  �ȉ��̃V�[�����܂܂�Ă��܂��B
  - bs_cube.edge: �u�����h�V�F�C�v���g�p�����ȒP�ȃL���[�u
  - bs_cube2.edge: �u�����h�V�F�C�v���g�p�����ȒP�ȃL���[�u
  - bs_skin_cyl.edge: �u�����h�V�F�C�v���g�p���X�L�j���O���ꂽ�V�����_�[
  - skin_cyl.edge: �X�L�j���O���ꂽ�V�����_�[
  - skin_cyl_customcompress.edge: ���_�f�[�^�ɑ΂��J�X�^���̈��k�E��
				�@�R�[�h���g�p���X�L�j���O���ꂽ�V�����_�[
  - teapot.edge: Edge �̃X�^�e�B�b�N�ȃW�I���g���̍œK���𗘗p�����A�T�C�Y
		 �̑傫���X�^�e�B�b�N�V�[��

  �ȈՉ��̂��߁A�A�j���[�V�����}�g���b�N�X�ƃu�����h�V�F�C�v�̃t�@�N�^�́A
�@PPU �ɂ����Ď菇�ɏ]���Čv�Z���܂��B�܂܂�Ă�����̈ȊO�̔C�ӂɃA�j���[
�@�V���������ꂽ�V�[���ɂ��ẮA�T���v�����Ή��ł��Ȃ����߂ł��B�`��͂�
�@���܂����A�������A�j���[�V����������Ȃ��ꍇ������܂��B

  �g�p���Ă���@�\�Ɋ֌W�Ȃ��AEdge Geometry SPURS �W���u���쐬����R�[�h
  �͓����ł��BEdge Geometry �ł̕`��́A���Ƀf�[�^�쓮�^�̏����ɂȂ��
�@���B�ǂ̋@�\��L���ɂ��邩�́AEdgeGeomSpuConfigInfo �\���̂̃R���e���c
�@�ɂ���đ唼�����܂�܂��B���̂��߁ARuntime �R�[�h�͗l�X�ȗp�r�Ɏg�p��
�@�邱�Ƃ��ł��܂��B

<�t�@�C��>
    Makefile           : Makefile
    bs_cube.dae        : �u�����h�V�F�C�v���g�p�����L���[�u�V�[��
			�iCollada �\�[�X�j
    bs_cube.edge       : �u�����h�V�F�C�v���g�p�����L���[�u�V�[��
			�i�o�C�i�� Edge �V�[���j
    bs_cube.edge.h     : �u�����h�V�F�C�v���g�p�����L���[�u�V�[��
			�iC �w�b�_ Edge �V�[���j
    bs_cube2.dae       : �u�����h�V�F�C�v���g�p���� cube2 �V�[��
			�iCollada �\�[�X�j
    bs_cube2.edge      : �u�����h�V�F�C�v���g�p���� cube2 �V�[��
			�i�o�C�i�� Edge �V�[���j
    bs_cube2.edge.h    : �u�����h�V�F�C�v���g�p���� cube2 �V�[��
			�iC �w�b�_ Edge �V�[���j
    bs_skin_cyl.dae    : �u�����h�V�F�C�v���g�p���X�L�j���O���ꂽ
			 �V�����_�[�V�[���iCollada �\�[�X�j
    bs_skin_cyl.edge   : �u�����h�V�F�C�v���g�p���X�L�j���O���ꂽ
			 �V�����_�[�V�[���i�o�C�i�� Edge �V�[���j
    bs_skin_cyl.edge.h : �u�����h�V�F�C�v���g�p���X�L�j���O���ꂽ
			 �V�����_�[�V�[���iC �w�b�_ Edge �V�[���j
    fpshader.cg        : �t���O�����g�V�F�[�_�v���O����
    logo-color.bin     : �S�V�[���Ŏg�p����J���[�e�N�X�`���i�����H��
			 �s�N�Z���f�[�^�j
    logo-normal.bin    : �S�V�[���Ŏg�p����@���}�b�v�e�N�X�`���i�����H
			 �̃s�N�Z���f�[�^�j
    main.cpp           : ���C���̃T���v���\�[�X
    main.h             : ���C���̃T���v���w�b�_
    readme_e.txt       : Readme �e�L�X�g�t�@�C���i�p���)
    readme_j.txt       : Readme �e�L�X�g�t�@�C���i���{���)
    skin_cyl.dae       : �X�L�j���O���ꂽ�V�����_�[�V�[��
			�iCollada �\�[�X�j
    skin_cyl.edge      : �X�L�j���O���ꂽ�V�����_�[�V�[��
			�i�o�C�i�� Edge �V�[���j
    skin_cyl.edge.h    : �X�L�j���O���ꂽ�V�����_�[�V�[��
			�iC �w�b�_ Edge �V�[���j
    skin_cyl_customcompress.dae   : �J�X�^�����k���g�p�����X�L�j���O����
				�@�@���V�����_�[�V�[���iCollada �\�[�X�j
    skin_cyl_customcompress.edge  : �J�X�^�����k���g�p�����X�L�j���O����
				�@�@���V�����_�[�V�[��
				�@�@�i�o�C�i�� Edge �V�[���j
    skin_cyl_customcompress.edge.h :�J�X�^�����k���g�p�����X�L�j���O����
				�@�@���V�����_�[�V�[��
				�@�@�iC �w�b�_ Edge �V�[���j
    teapot.dae         : �X�^�e�B�b�N�ȃe�B�[�|�b�g�V�[��
			�iCollada �\�[�X�j
    teapot.edge        : �X�^�e�B�b�N�ȃe�B�[�|�b�g�V�[��		
			�i�o�C�i�� Edge �V�[���j
    teapot.edge.h      : �X�^�e�B�b�N�ȃe�B�[�|�b�g�V�[��
			 �iC �w�b�_ Edge �V�[���j
    vpshader.cg        : ���_�V�F�[�_�v���O����
    spu/job_geom.cpp   : Edge Geometry SPURS �W���u�\�[�X
    spu/job_geom.mk    : Edge Geometry SPURS �W���u�� makefile

<�v���O�����̎��s���@>
    - �R�}���h���C����1������n���ăT���v�������s���܂��B
�@�@�@�ǂݍ��ރo�C�i�� .edge �V�[���t�@�C���ւ̃p�X���w�肵�܂��B
