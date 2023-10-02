#include <MediaPlayer.h>
#include <OutputMixer.h>
#include <MemoryUtil.h>

const int MIXER_VOLUME = -160;
//バッファのサイズ10^13（なんで？）
const int32_t S_BUFFER_SIZE = 8192;
//8bit unsigned int
uint8_t s_buffer[S_BUFFER_SIZE];
bool err_flag = false;
MediaPlayer *player;
OutputMixer *mixer;

static void error_callback(const ErrorAttentionParam *errparam) {
  if (errparam->error_code > AS_ATTENTION_CODE_WARNING) {
    err_flag = true;
  }
}

static void mixer_done_callback(MsgQueId id, MsgType type, AsOutputMixDoneParam *param) {
  return;
}

static void player_decode_callback(AsPcmDataParam pcm_param) {
  int16_t *ls = (int16_t*)pcm_param.mh.getPa();
}

void setup() {
  // put your setup code here, to run once:

  initMemoryPools();
  //共有メモリのレイアウトを設定。再生機能用にレイアウト
  createStaticPools(MEM_LAYOUT_PLAYER);
  player = MediaPlayer::getInstance();
  mixer = OutputMixer::getInstance();
  //プレイヤーの初期化(メモリ確保)
  player->begin();
  //オーディオハードウェアを有効化
  mixer->activateBaseband();
  //プレイヤーオブジェクト作成
  player->create(MediaPlayer::Player0, error_callback);
  //ミキサーを有効化
  mixer->activate(OutputMixer0, mixer_done_callback);
  usleep(100 * 1000);
  //コーデックとかを決める
  player->init(MediaPlayer::Player0, AS_CODECTYPE_WAV, "/mnt/sd0/BIN", AS_SAMPLINGRATE_48000, AS_BITLENGTH_16, AS_CHANNEL_STEREO);
  mixer->setVolume(MIXER_VOLUME, 0, 0);

  //ゼロ埋め
  memset(s_buffer, 0, sizeof(s_buffer));
  //Player0にs_bufferを渡す
  //Playerは2個ある そのうちの0個目を指定
  player->writeFrames(MediaPlayer::Player0, s_buffer, S_BUFFER_SIZE);
  //再生開始! writeFramesで渡されたものを再生する
  player->start(MediaPlayer::Player0, player_decode_callback);
}

void loop() {
  // put your main code here, to run repeatedly:


  player->writeFrames(MediaPlayer::Player0, s_buffer, S_BUFFER_SIZE);
}
