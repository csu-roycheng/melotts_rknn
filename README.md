# melotts_rknn
MeloTTS RKNN implementation.

- wetextprocessing, cppjieba, cppinyin for chinese text frontend and phonemizer
- replace bert-base-multilingual-uncased with albert-tiny, and convert to ONNX
- split model into encoder and decoder, convert encoder to ONNX and decoder to RKNN fp16

Acknowledgment:
- https://github.com/wenet-e2e/WeTextProcessing
- https://github.com/yanyiwu/cppjieba
- https://github.com/pkufool/cppinyin
- https://github.com/apinge/MeloTTS.cpp
- https://github.com/ml-inory/melotts.axera
