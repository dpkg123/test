[SCE CONFIDENTIAL DOCUMENT]
PlayStation(R)Edge 1.2.0
                  Copyright (C) 2007 Sony Computer Entertainment Inc.
                                                 All Rights Reserved.

<�T���v���v���O�����̃|�C���g>

�{�T���v���́A���C���[�t���[���X�P���g���̃A�j���[�V������`��� 
Edge Animation ���g�p����ꍇ�̊ȒP�ȕ��@�������܂��BPPU �R�[�h�́A
�L�����N�^�[����1�̃u�����h�c���[���\�z���A�ʏ�� SPURS �W���u�̃Z�b�g
�A�b�v��\�����܂��BSPU �R�[�h�́A�u�����h�c���[���������A���C����������
�`���߂��O�ɁA���̌��ʂ𕡐��̃��[���h�X�y�[�X�}�g���b�N�X�ɕϊ����܂��B

<�T���v���v���O�������>

�ȉ��ɁA���C���֐��̏ڍׂ������܂��B

SampleApp::onInit()

	SPURS �����������܂��B
	Edge Animation �����������܂��BSPU ���� 128k �̊O���|�[�Y
        �L���b�V�������������蓖�Ă��܂��B�{�T���v�����ł͔��ɒP����
�@�@�@�@�u�����h�c���[���g�p���Ă��邽�߁A�{���͊O���|�[�Y�L���b�V����
�@�@�@�@�K�v�Ƃ��܂���B���̂��߁A�{�T���v���͏����Ƀf�����X�g���[�V����
�@�@�@�@������ړI�ɍ쐬����Ă��܂��B
	�X�P���g����4�̃A�j���[�V�����o�C�i�������[�h���܂��i2��
�@�@�@�@�A�C�h���� walk �� jog �e1�j�B
	�e�L�����N�^�[�ɂ́A���[���h�}�g���b�N�X�z�񂪊��蓖�Ă��܂��B
�@�@�@�@�����̔z��́ASPU �W���u�ɂ���ăt���[�����ɍX�V����܂��B

SampleApp::onUpdate()

	�]�����ԂƍŏI�u�����h�t�@�N�^�����P����܂����B�L�����N�^�[�́A
�@�@�@�@idle ���� walk �܂��� jog �ցA�܂� idle �ւƏ��X�Ƀu�����h���s��
�@�@�@�@��܂��B
�@�@�@�@�u�����h�c���[���Ŏg�p����Ă��鑼��2�̃u�����h�t�@�N�^�́i2��
�@�@�@�@�� idle �܂��� walk �� jog �j�A�L�����N�^�[���Ɉ��ł��̂Œ��ӂ�
        �Ă��������B

SampleApp::runAnimJobs()

	�e�L�����N�^�[�ɑ΂��AcreateBlendTree() �� createAnimJob() ���Ă�
	�o���܂��B�]�����Ԃƃu�����h�t�@�N�^�ɂ��ẮA�W�b�^�������s����
�@�@�@�@�L�����N�^�[���ɕω���^���܂��B�W���u�̓L�b�N����A���̃T���v��
	�͌��ʂ��o��܂ŃX�g�[�����܂��B
    
SampleApp::createTestBlendTree()

	�u�����h�t�@�N�^�ɓn���ꂽ���̂��g�p����4�̃A�j���[�V������
	�u�����h����V���v���ȃu�����h�c���[���쐬���܂��Bwalk �� jog ��
	�T�C�N���͂��ꂼ��قȂ�A�ʏ�̓u�����f�B���O���ɓ�������܂�
	���A�ȈՉ��̂��߂ɂ����ł͍̗p���Ă��܂���B

SampleApp::createAnimJob()

	�u�����h�c���[��]������ SPU �W���u��1�쐬���A�����̃��[���h
	�}�g���b�N�X�̔z��ɏo�͂��܂��BSPURS �W���u�͈ȉ��̂悤�ɃZ�b�g
	�A�b�v����܂��B

    ���̓o�b�t�@	: �u�����h�c���[�̃u�����`�ƃ��[�t�̔z��
    �L���b�V���o�b�t�@	: �X�P���g���ƃO���[�o���̃R���e�L�X�g
�@�@�@�@�@�@�@�@�@�@�@�@�i�����͑S�L�����N�^�[�ŋ��L�����)
    �o�̓o�b�t�@	: �]���ς݂̃��[���h�}�g���b�N�X�z��
    �X�N���b�`�o�b�t�@	: Edge Animation �p�� 128K ���\�񂳂��
�@�@�p�����[�^		: ���[�g�W���C���g�A�u�����h�u�����`�J�E���g�A
			�@�u�����h���[�t�J�E���g�A�o�̓}�g���b�N�X�z���
			�@�L���A�h���X

cellSpursJobMain2()

	���̃W���u�̃G���g���|�C���g�B�W���u�p�����[�^�ƑS�o�b�t�@�ւ�
	�|�C���^���擾���܂��BprocessBlendTree() ���Ăяo���Ă��̏������s
	���A�ŏI�̃��[���h�}�g���b�N�X�����C���������ɏ����߂��܂��B

processBlendTree()

	edgeAnimSpuInitialize() ���Ăяo���āAEdge Animation SPU ���C�u��
�@�@�@�@�������������܂��B�܂��AedgeAnimProcessBlendTree() ���Ăяo���āA
�@�@�@�@�u�����h�c���[���������܂��B �u�����h�c���[�̃��[�g�̓u�����`�Ȃ�
�@�@�@�@�ŁAEDGE_ANIM_BLEND_TREE_INDEX_BRANCH �����[�g�t���O�Ƃ��ēn����
�@�@�@�@���B
	���̒i�K�ł́A�]���ς݂̃��[�J���X�y�[�X�W���C���g�̓|�[�Y�X�^�b
�@�@�@�@�N�̈�ԏ�ɑ��݂��܂��B
	�G���g���́A���[���h�X�y�[�X�W���C���g�p�Ƀ|�[�Y�X�^�b�N���
	�v�b�V������A�ϊ����s�����߂� edgeAnimLocalJointsToWorldJoints()
�@�@�@�@���Ăяo����܂��B
	edgeAnimJointsToMatrices4x4() ���Ăяo���āA���[���h�W���C���g��
	�}�g���b�N�X�ɕϊ����܂��i�o�̓o�b�t�@�ɒ��ڍs���j�B
	�Ō�ɁA�|�[�Y�X�^�b�N�����Z�b�g���AEdge Animation SPU ���C�u����
�@�@�@�@���������܂��B


<�t�@�C��>
main.cpp			: ���C�� PPU �R�[�h�i�W���u�Z�b�g�A�b�v�A
				�@�u�����h�c���[�Z�b�g�A�b�v�Ȃǂ��܂ށj
spu/process_blend_tree.cpp	: ���C���A�j���[�V���������֐�
				�i�X�v���b�g���� SPU �W���u�܂��� 
				 PC/win32�ł̂����ꂩ����Ăяo���j
spu/job_anim.cpp		: �A�j���[�V�����W���u���C���֐�
spu/job_anim.mk			: �A�j���[�V�����W���u�� makefile
assets/*.dae			: ���s���f�[�^�̍\�z���Ɏg�p���� Collada 
				  �̃\�[�X�t�@�C��
assets.mk			: �v���C�}���� makefile �ɂ���Đ������ꂽ
				�@���s���f�[�^���\�z���� makefile
 
<�v���O�����̎��s���@>
    - �t�@�C���T�[�r���O�̃��[�g�� self �f�B���N�g���ɐݒ肵�܂��B
    - �v���O���������s���܂��B
