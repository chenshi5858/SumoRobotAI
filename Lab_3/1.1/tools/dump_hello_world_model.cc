#include <cstdint>
#include <cstdio>

#include "examples/hello_world/main/model.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {

void PrintShape(const flatbuffers::Vector<int32_t> *shape)
{
    std::printf("[");
    if (shape != nullptr) {
        for (uint32_t i = 0; i < shape->size(); ++i) {
            std::printf("%s%d", i == 0 ? "" : ",", shape->Get(i));
        }
    }
    std::printf("]");
}

void PrintQuant(const tflite::QuantizationParameters *quant)
{
    if (quant == nullptr) {
        std::printf("quant=<none>");
        return;
    }

    std::printf("scale=");
    const auto *scales = quant->scale();
    if (scales == nullptr) {
        std::printf("[]");
    } else {
        std::printf("[");
        for (uint32_t i = 0; i < scales->size(); ++i) {
            std::printf("%s%.10g", i == 0 ? "" : ",", scales->Get(i));
        }
        std::printf("]");
    }

    std::printf(" zp=");
    const auto *zero_points = quant->zero_point();
    if (zero_points == nullptr) {
        std::printf("[]");
    } else {
        std::printf("[");
        for (uint32_t i = 0; i < zero_points->size(); ++i) {
            std::printf("%s%lld", i == 0 ? "" : ",", static_cast<long long>(zero_points->Get(i)));
        }
        std::printf("]");
    }
}

void PrintBufferBytes(const tflite::Buffer *buffer)
{
    const auto *data = buffer->data();
    if (data == nullptr) {
        std::printf("  data=[]\n");
        return;
    }

    std::printf("  data_len=%u\n  data=[", data->size());
    for (uint32_t i = 0; i < data->size(); ++i) {
        std::printf("%s%d", i == 0 ? "" : ",", static_cast<int8_t>(data->Get(i)));
    }
    std::printf("]\n");
}

}  // namespace

int main()
{
    const tflite::Model *model = tflite::GetModel(g_model);
    const tflite::SubGraph *subgraph = model->subgraphs()->Get(0);

    std::printf("version=%d\n", model->version());
    std::printf("operators=%u tensors=%u buffers=%u\n\n",
                subgraph->operators()->size(),
                subgraph->tensors()->size(),
                model->buffers()->size());

    std::printf("inputs:");
    for (uint32_t i = 0; i < subgraph->inputs()->size(); ++i) {
        std::printf(" %d", subgraph->inputs()->Get(i));
    }
    std::printf("\noutputs:");
    for (uint32_t i = 0; i < subgraph->outputs()->size(); ++i) {
        std::printf(" %d", subgraph->outputs()->Get(i));
    }
    std::printf("\n\n");

    for (uint32_t i = 0; i < subgraph->tensors()->size(); ++i) {
        const tflite::Tensor *tensor = subgraph->tensors()->Get(i);
        std::printf("tensor %u name=%s type=%d shape=",
                    i,
                    tensor->name() == nullptr ? "" : tensor->name()->c_str(),
                    tensor->type());
        PrintShape(tensor->shape());
        std::printf(" buffer=%u ", tensor->buffer());
        PrintQuant(tensor->quantization());
        std::printf("\n");
        PrintBufferBytes(model->buffers()->Get(tensor->buffer()));
    }

    std::printf("\n");
    for (uint32_t i = 0; i < subgraph->operators()->size(); ++i) {
        const tflite::Operator *op = subgraph->operators()->Get(i);
        const tflite::OperatorCode *opcode = model->operator_codes()->Get(op->opcode_index());
        std::printf("op %u builtin=%d inputs=", i, opcode->builtin_code());
        for (uint32_t j = 0; j < op->inputs()->size(); ++j) {
            std::printf("%s%d", j == 0 ? "[" : ",", op->inputs()->Get(j));
        }
        std::printf("] outputs=");
        for (uint32_t j = 0; j < op->outputs()->size(); ++j) {
            std::printf("%s%d", j == 0 ? "[" : ",", op->outputs()->Get(j));
        }
        std::printf("]");

        if (op->builtin_options_type() == tflite::BuiltinOptions_FullyConnectedOptions) {
            const auto *opts = op->builtin_options_as_FullyConnectedOptions();
            std::printf(" activation=%d weights_format=%d keep_num_dims=%d",
                        opts->fused_activation_function(),
                        opts->weights_format(),
                        opts->keep_num_dims());
        }
        std::printf("\n");
    }

    return 0;
}
