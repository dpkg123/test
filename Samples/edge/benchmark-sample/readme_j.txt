[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<�T���v���v���O�����̃|�C���g>

�{�T���v���́A�A�j���[�V����������уX�L�������ꂽ�L�����N�^�[��\������
���߂� Edge Animation �� Edge Geometry �̗������g�p�����ꍇ�̗�������܂��B
<�T���v���v���O�������>

�A�j���[�V�����W���u�́A�t���[���̂͂��߂ɍ쐬����i�L�����N�^�[����1�j
�L�b�N�I�t����܂��B�A�j���[�V�����W���u���̂́Aanim-sample �Ƃقړ����ŁA
4x4 �̃��[���h�}�g���b�N�X�ł͂Ȃ��A3x4 �̃X�L�j���O�}�g���b�N�X���o�͂���
�_���قȂ�܂��B���̃}�g���b�N�X�́A��� Edge Geometry ���A3x4 �̃��[���h
�}�g���b�N�X����� 3x4 �C���o�[�X�o�C���h�|�[�Y�}�g���b�N�X�̏�Z���v�Z��
��ۂɎg�p���܂��B�i���F�C���o�[�X�o�C���h�|�[�Y�z��́AedgeGeomCompiler 
�ɂ���ĕʃt�@�C���Ƃ��ăG�N�X�|�[�g����܂��B�j

���̃T���v���́A�A�j���[�V�����̃W���u���I���܂ŃX�g�[�����A�e�L�����N
�^�[�Ɋ֘A�t����ꂽ�X�L�j���O�}�g���b�N�X���g���ăW�I���g���W���u���L�b
�N�I�t���܂��B

�A�j���[�V�����W���u�ƃW�I���g���W���u���Z�b�g�A�b�v����ю��s����R�[�h
�́Aanim-sample �� geom-sample �Ƃقړ����ł��B�ڍׂɂ��ẮA�����̃T
���v�����Q�Ƃ��Ă��������B

<�t�@�C��>
main.cpp		: ���C�� PPU �R�[�h�i�W���u�Z�b�g�A�b�v�A�u�����h
			�@�c���[�Z�b�g�A�b�v�Ȃǂ��܂�)
spu/job_anim.cpp	: �A�j���[�V�����W���u
spu/job_anim.mk		: �A�j���[�V�����W���u�� makefile
spu/job_geom.cpp	: �W�I���g���W���u
spu/job_geom.mk		: �W�I���g���W���u�� makefile
assets/*.dae		: ���s���f�[�^�̍\�z���Ɏg�p���� Collada 
			  �̃\�[�X�t�@�C��
assets.mk		: �v���C�}���� makefile �ɂ���Đ������ꂽ
			�@���s���f�[�^���\�z���� makefile
 
<�v���O�����̎��s���@>
    - �t�@�C���T�[�r���O�̃��[�g�� self �f�B���N�g���ɐݒ肵�܂��B
    - �v���O���������s���܂��B
