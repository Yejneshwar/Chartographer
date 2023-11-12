

#include "ChartographMessenger.grpc.pb.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include <AbstractApplication.h>
#include "Renderer/BatchRenderer.h"

struct lines {
	std::vector<glm::vec3> points;
	std::vector<unsigned int> indices;

	void addPoint(glm::vec3 point) {
		points.push_back(point);
		
		if (points.size() == 1) {
			return;
		}
		indices.push_back(((unsigned int)points.size() - 2));
		indices.push_back(((unsigned int)points.size() - 1));
	}
};

static std::map<std::string, lines> plots;

class ChartoGraphMessengerImpl : public ChartographMessenger::GraphPlotter::Service {
	grpc::Status CreatePlot(grpc::ServerContext* context, const ChartographMessenger::GraphData* request, ChartographMessenger::PlotResponse* response) override {
		std::string message("Plot Created");

		if (request->plotid().empty()) {
			return plotIdError(response);
		}

		plots.insert({ request->plotid(), lines()});

		response->set_plotid(request->plotid());
		response->set_success(true);
		response->set_message(message);
		return grpc::Status::OK;
	}

	grpc::Status UpdatePlot(grpc::ServerContext* context, const ChartographMessenger::UpdateData* request, ChartographMessenger::PlotResponse* response) override {
		
		if (request->plotid().empty()) {
			return plotIdError(response);
		}

		for (ChartographMessenger::Point point : request->new_data_points()) {
			plots[request->plotid()].addPoint(glm::vec3(point.x(),point.y(),0.0f));
		}
		std::string message("Point added");
		response->set_plotid(request->plotid());
		response->set_success(true);
		response->set_message(message);
		return grpc::Status::OK;
	}
private:
	inline grpc::Status plotIdError(ChartographMessenger::PlotResponse* response) {
		return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Plot Id");
	}
};

static std::shared_ptr<grpc::Channel> serverChannel;

class ObjectLayer : public GUI::Layer {
public:
	ObjectLayer() : Layer("ObjectLayer") {}
	~ObjectLayer() {}

	void OnAttach() {}

	void OnDrawUpdate() {
		for (const auto& plot : plots) {
			Graphics::BatchRenderer::DrawLines(plot.second.points, plot.second.indices,glm::vec4(1.0));
		}
	}

	void OnImGuiRender() {
		ImGui::Begin("Plots");
		for (const auto& plot : plots) {
			if (ImGui::TreeNode(plot.first.c_str())) {
				for (const auto& point : plot.second.points) {
					ImGui::Text("X : %f , Y : %f", point.x, point.y);
				}
				ImGui::TreePop();

			}
		}
		ImGui::End();
	}
};

class ChartoGrapher : public GUI::AbstractApplication {
private:
public:
	ChartoGrapher(const GUI::ApplicationSpecification& spec)
		:AbstractApplication(spec)
	{
		PushLayer(new ObjectLayer());
	}

	~ChartoGrapher() {
		std::cout << "ChartoGrapher Destructor" << std::endl;
	}
};


ChartoGrapher* CreateApplication(GUI::ApplicationCommandLineArgs args)
{
	GUI::ApplicationSpecification spec;
	spec.Name = "ChartoGrapher";
	spec.CommandLineArgs = args;
	return new ChartoGrapher(spec);
}


int main(int argc, char** argv)

{
	auto app = CreateApplication({ argc, argv });
	//Note: Reflection in postman will not work with this line
	//grpc::EnableDefaultHealthCheckService(true);
	grpc::reflection::InitProtoReflectionServerBuilderPlugin();

	std::string server_address("0.0.0.0:50051");
	ChartoGraphMessengerImpl service;

	grpc::ServerBuilder builder;
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	builder.RegisterService(&service);
	std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
	grpc::ChannelArguments args;
	serverChannel = server->InProcessChannel(args);

	LOG_INFO_STREAM << "Server listening on " << server_address;
	//server->Wait();
	app->Run();

	delete app;
}
