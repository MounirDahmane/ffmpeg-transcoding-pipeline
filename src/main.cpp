extern "C"{
#include<stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
}

#define LOG_ERROR(...) do { \
    fprintf(stderr, "\033[1;31m[ERROR] %s:%d (%s): ", __FILE__, __LINE__, __func__); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\033[0m\n"); \
    exit(1); \
} while(0)

static void encode(AVCodecContext *codec_ctx, AVFrame *frame, AVPacket *pkt, AVFormatContext *s);
static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, int nb_frame);

struct VideoEncoderCtx{
const AVCodec *enc;
AVCodecContext *enc_ctx;
AVFormatContext *out_fmt_ctx;
AVFrame *frame;
AVPacket *pkt;
AVStream *out_stream;
}videoContext;

int main(){

{
    // --------------------------------- encoding
    const AVCodec *enc;
    AVCodecContext *enc_ctx;
    AVFormatContext *out_fmt_ctx;
    AVFrame *frame;
    AVPacket *pkt;
    AVStream *out_stream;

    const char *out_filename = "./output_H.264.mp4";
    int ret = 0;

    enc = avcodec_find_encoder_by_name("libx264");
    if(!enc){
        fprintf(stderr, "ERROR: Cannot find the encoder '%s'\n", enc->long_name);
        exit(1);
    }
    enc_ctx = avcodec_alloc_context3(enc);
    if(!enc_ctx){
        fprintf(stderr, "ERROR: Cannot allocate the encoder context\n");
        exit(1);
    }

    av_opt_set(enc_ctx->priv_data, "preset", "slow", 0);
    av_opt_set(enc_ctx->priv_data, "crf", "18", 0);

    enc_ctx->width        = 360;
    enc_ctx->height       = 360;
    enc_ctx->framerate    = AVRational{25, 1};
    enc_ctx->time_base    = av_inv_q(enc_ctx->framerate);
    enc_ctx->pix_fmt      = AV_PIX_FMT_YUV420P;
    enc_ctx->gop_size     = 10;
    enc_ctx->max_b_frames = 1;

    ret = avcodec_open2(enc_ctx, enc, nullptr);
    if(ret < 0){
        LOG_ERROR("could not open the encoder");
    }

    ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, out_filename);
    if(ret < 0){
        LOG_ERROR("could not allocate output format context");   
    }

    out_stream = avformat_new_stream(out_fmt_ctx, nullptr);
    if(!out_stream){
        LOG_ERROR("could not create the new stream");
    }

    out_stream->time_base = enc_ctx->time_base;
    ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    if(ret < 0){
        LOG_ERROR("could not pass parameters to the stream from the context");
    }

    ret = avio_open(&out_fmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
    if(ret < 0){
        LOG_ERROR("could not open the output file");
    }
    ret = avformat_write_header(out_fmt_ctx, nullptr);
    if(ret < 0){
        LOG_ERROR("could not write the header");
    }

    frame = av_frame_alloc();
    if(!frame){
        LOG_ERROR("could not allocate the frame");
    }
    frame->height = enc_ctx->height;
    frame->width  = enc_ctx->width;
    frame->format = enc_ctx->pix_fmt;
    
    pkt = av_packet_alloc();
    if(!pkt){
        LOG_ERROR("could not allocate packet");
    }

    ret = av_frame_get_buffer(frame, 0);
    if(ret < 0){
        LOG_ERROR("could not get frame buffer");
    }

    for(int i=0; i < 2500; i++){

        ret = av_frame_make_writable(frame);
        if(ret < 0){
            LOG_ERROR("could not make the frame writable");
        }

        for (int y = 0; y < frame->height; y++) {
            uint8_t* row = frame->data[0] + y * frame->linesize[0];
            for (int x = 0; x < frame->width; x++) {
                row[x] = (x + y + i * 3);
            }
        }

        for (int y = 0; y < frame->height / 2; y++) {
            uint8_t* u_row = frame->data[1] + y * frame->linesize[1];
            uint8_t* v_row = frame->data[2] + y * frame->linesize[2];
            for (int x = 0; x < frame->width / 2; x++) {
                u_row[x] = (128 + (x + y + i * 3)) % 256;  
                v_row[x] = (64  + (x + y + i * 3)) % 256;
            }
        }

        frame->pts = i;

        encode(enc_ctx, frame, pkt, out_fmt_ctx);
    }

    encode(enc_ctx, nullptr, pkt, out_fmt_ctx);

    av_write_trailer(out_fmt_ctx);        
    av_dump_format(out_fmt_ctx, 1, out_filename, 1);

    avio_closep(&out_fmt_ctx->pb);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&enc_ctx);
    avformat_free_context(out_fmt_ctx);

}
    //--------------------extracting gray frames -> ffmpeg unknown hello world
{
    int ret =0;
    const AVCodec *dec;
    AVCodecContext *dec_ctx;
    AVFormatContext *inp_fmt_ctx;
    AVCodecParameters *dec_par;
    AVFrame *out_frame;
    AVPacket *out_pkt;
    AVStream *stream;
    const char *inp_filename = "./output_H.264.mp4";
    inp_fmt_ctx = avformat_alloc_context();
    if(!inp_fmt_ctx){
        LOG_ERROR("could not allocat input format");
    }

    ret = avformat_open_input(&inp_fmt_ctx, inp_filename, nullptr, nullptr);
    if(ret < 0){
        LOG_ERROR("could not open the input format context file");
    }
    ret = avformat_find_stream_info(inp_fmt_ctx, nullptr);
    if(ret < 0){
        LOG_ERROR("could not find the stream info");
    }
    int video_stream = -1;

    for(uint8_t i=0; i<inp_fmt_ctx->nb_streams; i++){
        AVStream *stream = inp_fmt_ctx->streams[i];
        AVCodecParameters *par = stream->codecpar;

        if(par->codec_type != AVMEDIA_TYPE_VIDEO)
            continue;

        video_stream = i;
        dec_par = par;
        dec = avcodec_find_decoder(par->codec_id);
        if(!dec){
            LOG_ERROR("could not find decoder");
        }
    }
    if(video_stream < 0){
        LOG_ERROR("there is no video stream inside the file: %s", inp_filename);
    }

    dec_ctx = avcodec_alloc_context3(dec);
    if(!dec_ctx){
        LOG_ERROR("could not allocate the decoder");
    }
    ret = avcodec_parameters_to_context(dec_ctx, dec_par);
    if(ret < 0){
        LOG_ERROR("could not pass the parameres to the decoder");
    }
    ret = avcodec_open2(dec_ctx, dec, nullptr);
    if(ret < 0){
        LOG_ERROR("could not open the decoder");
    }

    out_frame = av_frame_alloc();
    if(!out_frame){
        LOG_ERROR("could not allocate the frame");
    }
    out_pkt = av_packet_alloc();
    if(!out_pkt){
        LOG_ERROR("could not allocate the packet");
    }

    uint8_t nb_frame = 0;

    while(av_read_frame(inp_fmt_ctx, out_pkt) >= 0){
        if(out_pkt->stream_index == video_stream){
            printf("AVPacket->pts %" PRId64, out_pkt->pts);printf("\n");
            ret = avcodec_send_packet(dec_ctx, out_pkt);
            if(ret < 0){
                LOG_ERROR("could not send the packet to the decoder");
            }
            ret = avcodec_receive_frame(dec_ctx, out_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                continue;
            else if (ret < 0) {
                LOG_ERROR("error while receiving frame");
            }

            save_gray_frame(out_frame->data[0], out_frame->linesize[0], out_frame->width, out_frame->height, nb_frame);

            nb_frame++;
            if(nb_frame > 10)
                break;
        }
        av_packet_unref(out_pkt);
    }

    avformat_close_input(&inp_fmt_ctx);
    avcodec_free_context(&dec_ctx);
    av_packet_free(&out_pkt);
    av_frame_free(&out_frame);

}
    //----------------------------------Transcoding to H.256
{
    AVFormatContext *in_fmt;
    AVFormatContext *out_fmt;
    const AVCodec * codec;
    AVCodecContext *dec;

    int ret = 0;
    int is_vd_stream = 0;
    AVPacket *packet;
    AVFrame *frame;
    AVStream *out_stream;

    const char *inp_filename = "./output_H.264.mp4";
    const char *out_filename = "./output_H.265.mp4";

    in_fmt = avformat_alloc_context();
    if(!in_fmt){
        LOG_ERROR("could not allocate input format context");
    }
    ret = avformat_open_input(&in_fmt, inp_filename, nullptr, nullptr);
    if(ret < 0){
        LOG_ERROR("could not open the input file");
    }
    ret = avformat_find_stream_info(in_fmt, nullptr);
    if(ret < 0){
        LOG_ERROR("could not find stream info");
    }

    ret = avformat_alloc_output_context2(&out_fmt, nullptr, nullptr, out_filename);
    if(ret < 0){
        LOG_ERROR("could not allocate output format context");
    }

    for(uint8_t i = 0; i < in_fmt->nb_streams; i++){
        AVStream *in_stream = in_fmt->streams[i];
        AVCodecParameters *par = in_stream->codecpar;

        if(par->codec_type != AVMEDIA_TYPE_VIDEO)
            continue;

        is_vd_stream = 1;
        codec = avcodec_find_decoder(par->codec_id);
        if(!codec){
            LOG_ERROR("could not find the decoder");
        }
        dec = avcodec_alloc_context3(codec);
        if(!dec){
            LOG_ERROR("could not allocate the decoder context");
        }
        ret = avcodec_parameters_to_context(dec, par);
        if(ret < 0){
            LOG_ERROR("could not copy parameters to context");
        }

        out_stream = avformat_new_stream(out_fmt, nullptr);
        if(!out_stream){
            LOG_ERROR("could not create new stream");
        }
        ret = avcodec_open2(dec, codec, nullptr);
        if(ret < 0){
            LOG_ERROR("could not open the decoder");
        }
    }
    if(!is_vd_stream){
        LOG_ERROR("there is no video stream to transcode");
    }
    
    const AVCodec *enc;
    AVCodecContext *enc_ctx;

    enc = avcodec_find_encoder_by_name("libx265");
    if(!enc){
        LOG_ERROR("coudl not fine the encoder");
    }
    enc_ctx = avcodec_alloc_context3(enc);
    if(!enc_ctx){
        LOG_ERROR("could not allocate the encoder context");
    }
    av_opt_set(enc_ctx->priv_data, "preset", "slow", 0);
    av_opt_set(enc_ctx->priv_data, "crf", "18", 0);

    enc_ctx->height       = dec->height;
    enc_ctx->width        = dec->width;
    enc_ctx->framerate    = dec->framerate;
    enc_ctx->time_base    = av_inv_q(dec->framerate);
    enc_ctx->gop_size     = dec->gop_size;
    enc_ctx->pix_fmt      = dec->pix_fmt;
    enc_ctx->max_b_frames = 1;
    
    avcodec_open2(enc_ctx, enc, nullptr);
    
    out_stream->time_base = enc_ctx->time_base;
    out_stream->codecpar->codec_tag = 0; // helps avoid muxing issues
    avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);

    ret = avio_open(&out_fmt->pb, out_filename, AVIO_FLAG_WRITE);
    if(ret < 0){
        LOG_ERROR("could not opent the output file");
    }
    ret = avformat_write_header(out_fmt, nullptr);
    if(ret < 0){
        LOG_ERROR("could not write the header on the output format");
    }

    packet = av_packet_alloc();
    if(!packet){
        LOG_ERROR("could not allcoate packet");
    }
    frame = av_frame_alloc();
    if(!frame){
        LOG_ERROR("could not allocate frame");
    }

    
    while(av_read_frame(in_fmt, packet) >= 0){
        ret = avcodec_send_packet(dec, packet);
        if(ret < 0){
            LOG_ERROR("could not send packet to decoder");
        }
        av_packet_unref(packet);
        while (ret >=0) {
            ret = avcodec_receive_frame(dec, frame);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if(ret < 0){
                LOG_ERROR("could not receive frame");
            }
            if(ret >= 0)
                encode(enc_ctx, frame, packet, out_fmt);
            av_frame_unref(frame);
        }
        av_packet_unref(packet);
    }

    av_dump_format(out_fmt, 1, out_filename, 1);
    
    encode(enc_ctx, NULL, packet, out_fmt);

    av_write_trailer(out_fmt);

    avio_closep(&out_fmt->pb);
    avcodec_free_context(&dec);
    av_frame_unref(frame);
    av_packet_unref(packet);

    avcodec_free_context(&enc_ctx);
    avformat_free_context(out_fmt);
    avformat_close_input(&in_fmt);
}

}

static void encode(AVCodecContext *codec_ctx, AVFrame *frame, AVPacket *pkt, AVFormatContext *s){
    int response = 0;
    response = avcodec_send_frame(codec_ctx, frame);
    if(response < 0){
        LOG_ERROR("could not send the frame to the encoder");
    }

    while(response >= 0){
        response = avcodec_receive_packet(codec_ctx, pkt);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
            return;
        else if (response < 0) {
            LOG_ERROR("error during encoding...");
        }

        av_interleaved_write_frame(s, pkt);
        av_packet_unref(pkt);
    }
}

static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, int nb_frame)
{
    FILE *f;
    int i;
    char filename[64];
    
    snprintf(filename, sizeof(filename), "./frames/frame%d.pgm", nb_frame);
    f = fopen(filename, "wb");
    if (!f) {
        perror("fopen");
        return;
    }
    // Write PGM header
    fprintf(f, "P5\n%d %d\n255\n", xsize, ysize);
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}