/* cocos2d for iPhone
 *
 * http://code.google.com/p/cocos2d-iphone
 *
 * Copyright (C) 2009 by Florin Dumitrescu.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the 'cocos2d for iPhone' license.
 *
 * You will find a copy of this license within the cocos2d for iPhone
 * distribution inside the "LICENSE" file.
 *
 *
 ***********************************************************************
 *
 * Part of the source code in this class has been provided by Apple.
 * For full distribution terms, see the top of MyOpenALSupport.h header file.
 *
 */

#import "PASoundSource.h"
#import "PASoundMgr.h"
#import "PASoundListener.h"
#import "ivorbiscodec.h"
#import "ivorbisfile.h"

#define kSoundReferenceDistance 20.0f

void* MyGetOpenALAudioData(CFURLRef inFileURL, ALsizei *outDataSize, ALenum *outDataFormat, ALsizei *outSampleRate)
{
	OSStatus						err = noErr;	
	UInt64							fileDataSize = 0;
	AudioStreamBasicDescription		theFileFormat;
	UInt32							thePropertySize = sizeof(theFileFormat);
	AudioFileID						afid = 0;
	void*							theData = NULL;
	
	// Open a file with ExtAudioFileOpen()
	err = AudioFileOpenURL(inFileURL, kAudioFileReadPermission, 0, &afid);
	if(err) { printf("MyGetOpenALAudioData: AudioFileOpenURL FAILED, Error = %ld\n", err); goto Exit; }
	
	// Get the audio data format
	err = AudioFileGetProperty(afid, kAudioFilePropertyDataFormat, &thePropertySize, &theFileFormat);
	if(err) { printf("MyGetOpenALAudioData: AudioFileGetProperty(kAudioFileProperty_DataFormat) FAILED, Error = %ld\n", err); goto Exit; }
	
	if (theFileFormat.mChannelsPerFrame > 2)  { 
		printf("MyGetOpenALAudioData - Unsupported Format, channel count is greater than stereo\n"); goto Exit;
	}
	
	if ((theFileFormat.mFormatID != kAudioFormatLinearPCM) || (!TestAudioFormatNativeEndian(theFileFormat))) { 
		printf("MyGetOpenALAudioData - Unsupported Format, must be little-endian PCM\n"); goto Exit;
	}
	
	if ((theFileFormat.mBitsPerChannel != 8) && (theFileFormat.mBitsPerChannel != 16)) { 
		printf("MyGetOpenALAudioData - Unsupported Format, must be 8 or 16 bit PCM\n"); goto Exit;
	}
	
	
	thePropertySize = sizeof(fileDataSize);
	err = AudioFileGetProperty(afid, kAudioFilePropertyAudioDataByteCount, &thePropertySize, &fileDataSize);
	if(err) { printf("MyGetOpenALAudioData: AudioFileGetProperty(kAudioFilePropertyAudioDataByteCount) FAILED, Error = %ld\n", err); goto Exit; }
	
	// Read all the data into memory
	UInt32		dataSize = (UInt32) fileDataSize;
	theData = malloc(dataSize);
	if (theData)
	{
		AudioFileReadBytes(afid, false, 0, &dataSize, theData);
		if(err == noErr)
		{
			// success
			*outDataSize = (ALsizei)dataSize;
			*outDataFormat = (theFileFormat.mChannelsPerFrame > 1) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
			*outSampleRate = (ALsizei)theFileFormat.mSampleRate;
		}
		else 
		{ 
			// failure
			free (theData);
			theData = NULL; // make sure to return NULL
			printf("MyGetOpenALAudioData: ExtAudioFileRead FAILED, Error = %ld\n", err); goto Exit;
		}	
	}
	
Exit:
	// Dispose the ExtAudioFileRef, it is no longer needed
	if (afid) AudioFileClose(afid);
	return theData;
}

@implementation PASoundSource

@synthesize file, extension, looped, isPlaying;

- (id)init {
    return nil;
}

// initializers with position
- (id)initWithPosition:(cpVect)pos file:(NSString *)f extension:(NSString *)e looped:(BOOL)yn {
    if ((self = [super init])) {
        self.file = f;
        self.extension = e;
        self.looped = yn;
        self.isPlaying = NO;
        [self initBuffer];
        [self initSource];
        self.position = pos;
        self.orientation = 0.0f;
        gain = 1.0f;
    }
    return self;
}
- (id)initWithPosition:(cpVect)pos file:(NSString *)f looped:(BOOL)yn{
    return [self initWithPosition:pos file:f extension:@"wav" looped:yn];
}
- (id)initWithPosition:(cpVect)pos file:(NSString *)f {
    return [self initWithPosition:pos file:f extension:@"wav" looped:NO];
}

// initializers without position (will have to be spcified at play time) -- defaulting to cpvzero
- (id)initWithFile:(NSString *)f extension:(NSString *)e looped:(BOOL)yn {
    return [self initWithPosition:cpvzero file:f extension:e looped:yn];
}
- (id)initWithFile:(NSString *)f looped:(BOOL)yn {
    return [self initWithPosition:cpvzero file:f extension:@"wav" looped:yn];
}
- (id)initWithFile:(NSString *)f {
    return [self initWithPosition:cpvzero file:f extension:@"wav" looped:NO];
}

- (void)initBuffer {
	ALenum  error = AL_NO_ERROR;
	ALenum  format;
	ALvoid* data = NULL;
	ALsizei size;
	ALsizei freq;
	
	NSBundle*				bundle = [NSBundle mainBundle];
	
	// get some audio data from a wave file
	CFURLRef fileURL = (CFURLRef)[[NSURL fileURLWithPath:[bundle pathForResource:self.file ofType:self.extension]] retain];
	
	if (fileURL) {
        
        // load buffer data based on the file format guessed from the extension
        if ([self.extension isEqualToString:@"wav"]) {
            // WAV
            data = MyGetOpenALAudioData(fileURL, &size, &format, &freq);            
        } else if ([self.extension isEqualToString:@"ogg"]) {
            // OGG
            NSString *fsPath = [(NSURL *)fileURL path];
            FILE *fh;
            if ((fh = fopen([fsPath UTF8String], "r")) != NULL) {
                // open ogg file
                OggVorbis_File vf;
                int eof = 0;
                int current_section;
                
                if(ov_open(fh, &vf, NULL, 0) < 0) {
                    fprintf(stderr,"Input does not appear to be an Ogg bitstream.\n");
                    exit(1);
                }
                
                // get meta info (sample rate & mono/stereo format)
                vorbis_info *vi = ov_info(&vf,-1);
                freq = (ALsizei)vi->rate;
                format = (vi->channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
                
                // decode data
                size = 0;
                const int buffSize = 4096;
                while(!eof){
                    char tempBuff[buffSize];
                    int ret = ov_read(&vf, tempBuff, buffSize, &current_section);
                    if (ret == 0) {
                        eof = 1;
                    } else if (ret > 0) {
                        void *newData = (char *)malloc(size + ret);
                        if (data) memcpy(newData, data, size);
                        memcpy(newData + size, tempBuff, ret);
                        if (data) free(data);
                        data = newData;
                        size += ret;
                    }
                }

                // close ogg file
                ov_clear(&vf);
            } else {
                fprintf(stderr,"Could not open file.\n");
                exit(1);
            }
            fclose(fh);
        }
		
        CFRelease(fileURL);
        
        if((error = alGetError()) != AL_NO_ERROR) {
			printf("error loading sound: %x\n", error);
			exit(1);
		}
        
		alGenBuffers(1, &buffer);
        alBufferData(buffer, format, data, size, freq);
		free(data);
        
		if((error = alGetError()) != AL_NO_ERROR) {
			printf("error attaching audio to buffer: %x\n", error);
		}		
	}
	else
		printf("Could not find file!\n");    
}

- (void)initSource {
    ALenum error = AL_NO_ERROR;
	alGetError(); // Clear the error
    
    alGenSources(1, &source);
    
	// Turn Looping ON?
    if (self.looped) {
        alSourcei(source, AL_LOOPING, AL_TRUE);        
    }
	
	// Set Source Reference Distance
	alSourcef(source, AL_REFERENCE_DISTANCE, kSoundReferenceDistance);
    
	// attach OpenAL Buffer to OpenAL Source
	alSourcei(source, AL_BUFFER, buffer);
	
	if((error = alGetError()) != AL_NO_ERROR) {
		printf("Error attaching buffer to source: %x\n", error);
		exit(1);
	}    
}

- (void)setGain:(float)g {
    gain = g;
    alSourcef(source, AL_GAIN, g * [[PASoundMgr sharedSoundManager] soundsMasterGain]);
}
- (void)setRolloff:(float)factor{
    alSourcef(source, AL_ROLLOFF_FACTOR, factor);
}
- (void)setPitch:(float)factor {
    alSourcef(source, AL_PITCH, factor);
}

// play messages
- (void)playAtPosition:(cpVect)p restart:(BOOL)r {
    cpVect currentPos = [self position];
    ALint state;
    if ((p.x != currentPos.x) || (p.y != currentPos.y)) {
        [self setPosition:p];
    }
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    if ((state == AL_PLAYING) && r) {
        // stop it before replaying
        [self stop];
        // get current state
        alGetSourcei(source, AL_SOURCE_STATE, &state);
    }
    if (state != AL_PLAYING) {
        alGetError();
        ALenum error;
        [self setGain:gain];
        alSourcePlay(source);
        if((error = alGetError()) != AL_NO_ERROR) {
            printf("error starting source: %x\n", error);
        } else {
            // Mark our state as playing (the view looks at this)
            self.isPlaying = YES;
        }        
    }    
}
- (void)playAtPosition:(cpVect)p {
    return [self playAtPosition:p restart:NO];
}
- (void)playWithRestart:(BOOL)r {
    return [self playAtPosition:self.position restart:r];
}
- (void)play {
    return [self playAtPosition:self.position restart:NO];    
}
- (void)playAtListenerPositionWithRestart:(BOOL)r {
    return [self playAtPosition:[[[PASoundMgr sharedSoundManager] listener] position] restart:r];
}
- (void)playAtListenerPosition {
    return [self playAtListenerPositionWithRestart:NO];
}

- (void)stop {
    alGetError();
    ALenum error;
	alSourceStop(source);
	if((error = alGetError()) != AL_NO_ERROR) {
		printf("error stopping source: %x\n", error);
	} else {
		// Mark our state as not playing (the view looks at this)
		self.isPlaying = NO;
	}    
}

- (cpVect)position {
    return position;
}
- (void)setPosition:(cpVect)pos {
    float x,y;
    position = pos;
    if ([[Director sharedDirector] landscape]) {
        x = pos.x - 240.0f;
        y = 160.0f - pos.y;
    } else {
        x = pos.x;
        y = pos.y;
    }    
    float sourcePosAL[] = {x, y, 0.0f};
	alSourcefv(source, AL_POSITION, sourcePosAL);
}

- (float)orientation {
    return orientation;
}
- (void)setOrientation:(float)o {
    orientation = o;
}


- (void)dealloc {
    [self stop];
    
    alGetError();
    ALenum error;
    alSourcei(source, AL_BUFFER, 0); // dissasociate buffer
    alDeleteBuffers(1, &buffer);
	if((error = alGetError()) != AL_NO_ERROR) {
		printf("error deleting buffer: %x\n", error);
    }
    alDeleteSources(1, &source);
    
    [super dealloc];
}

@end
