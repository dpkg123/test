#!/usr/bin/make
# FCDocument Makefile for PS3

include ../MakeDefs

DIR=FCDocument/

LIBNAME = libfcdocument

prefix=..

LIBPATH=$(prefix)/lib

PPU_INCDIRS=-I.. -I../LibXML/include
PPU_OPTIMIZE_LV= -O3 -funroll-loops

PPU_LIB_TARGET=$(LIBNAME).a
PPU_SRCS = \
	../FCollada.cpp \
	FCDAnimated.cpp \
	FCDAnimation.cpp \
	FCDAnimationChannel.cpp \
	FCDAnimationClip.cpp \
	FCDAnimationClipTools.cpp \
	FCDAnimationCurve.cpp \
	FCDAnimationCurveTools.cpp \
	FCDAnimationKey.cpp \
	FCDAnimationMultiCurve.cpp \
	FCDAsset.cpp \
	FCDCamera.cpp \
	FCDController.cpp \
	FCDControllerInstance.cpp \
	FCDControllerTools.cpp \
	FCDEffect.cpp \
	FCDEffectCode.cpp \
	FCDEffectParameter.cpp \
	FCDEffectParameterFactory.cpp \
	FCDEffectParameterSampler.cpp \
	FCDEffectParameterSurface.cpp \
	FCDEffectPass.cpp \
	FCDEffectPassState.cpp \
	FCDEffectPassShader.cpp \
	FCDEffectProfile.cpp \
	FCDEffectProfileFX.cpp \
	FCDEffectStandard.cpp \
	FCDEffectTechnique.cpp \
	FCDEffectTools.cpp \
	FCDEmitter.cpp \
	FCDEmitterInstance.cpp \
	FCDEmitterObject.cpp \
	FCDEmitterParticle.cpp \
	FCDEntity.cpp \
	FCDEntityInstance.cpp \
	FCDEntityReference.cpp \
	FCDExternalReferenceManager.cpp \
	FCDExtra.cpp \
	FCDForceDeflector.cpp \
	FCDForceDrag.cpp \
	FCDForceField.cpp \
	FCDForceGravity.cpp \
	FCDForcePBomb.cpp \
	FCDForceWind.cpp \
	FCDGeometry.cpp \
	FCDGeometryInstance.cpp \
	FCDGeometryMesh.cpp \
	FCDGeometryNURBSSurface.cpp \
	FCDGeometryPolygons.cpp \
	FCDGeometryPolygonsInput.cpp \
	FCDGeometryPolygonsTools.cpp \
	FCDGeometrySource.cpp \
	FCDGeometrySpline.cpp \
	FCDImage.cpp \
	FCDLibrary.cpp \
	FCDLight.cpp \
	FCDLightTools.cpp \
	FCDMaterial.cpp \
	FCDMaterialInstance.cpp \
	FCDMorphController.cpp \
	FCDObject.cpp \
	FCDObjectWithId.cpp \
	FCDParameterAnimatable.cpp \
	FCDParticleModifier.cpp \
	FCDPhysicsAnalyticalGeometry.cpp \
	FCDPhysicsForceFieldInstance.cpp \
	FCDPhysicsMaterial.cpp \
	FCDPhysicsModel.cpp \
	FCDPhysicsModelInstance.cpp \
	FCDPhysicsRigidBody.cpp \
	FCDPhysicsRigidBodyInstance.cpp \
	FCDPhysicsRigidBodyParameters.cpp \
	FCDPhysicsRigidConstraint.cpp \
	FCDPhysicsRigidConstraintInstance.cpp \
	FCDPhysicsShape.cpp \
	FCDPhysicsScene.cpp \
	FCDPlaceHolder.cpp \
	FCDSceneNode.cpp \
	FCDSceneNodeIterator.cpp \
	FCDSceneNodeTools.cpp \
	FCDSkinController.cpp \
	FCDTargetedEntity.cpp \
	FCDTexture.cpp \
	FCDTransform.cpp \
	FCDVersion.cpp \
	FCDocument.cpp \
	FCDocumentTools.cpp

include ../MakeRules
