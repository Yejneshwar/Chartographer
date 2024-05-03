

#include "ChartographMessenger.grpc.pb.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include <AbstractApplication.h>
#include "Renderer/BatchRenderer.h"

enum IndexMode {
	CONTINUOUS,
	DISCRETE
};

struct lines {
	std::vector<glm::vec3> points;
	std::vector<unsigned int> indices;

	void switchIndexMode() {
		if (indexMode == CONTINUOUS) {
			indexMode = DISCRETE;
			indices.clear();
			for (int i = 0; i < points.size(); i++) {
				indices.push_back(i);
			}
		}
		else if (indexMode == DISCRETE) {
			indexMode = CONTINUOUS;
			indices.clear();
			if (points.size() == 1) return;
			for (int i = 0; i < points.size() - 1; i++) {
				indices.push_back(((unsigned int)i));
				indices.push_back(((unsigned int)i + 1));
			}
		}
	}

	IndexMode getIndexMode() {
		return indexMode;
	}

	void addPoint(glm::vec3 point) {
		points.push_back(point);
		
		if (points.size() == 1 && indexMode == CONTINUOUS) {
			return;
		}
		addIndex();
	}
private:
	IndexMode indexMode = CONTINUOUS;

	void addIndex() {
		if (indexMode == CONTINUOUS) {
			indices.push_back(((unsigned int)points.size() - 2));
			indices.push_back(((unsigned int)points.size() - 1));
		}
		else if (indexMode == DISCRETE) {
			indices.push_back(((unsigned int)points.size() - 1));
		}
	}
};



struct triangles {
	std::set<glm::vec3, Graphics::BatchRenderer::Vec3Compare> points;
	std::vector<uint32_t> indices;
	std::vector<glm::vec4> colors;

	void addTriangle(glm::vec3 point1, glm::vec3 point2, glm::vec3 point3, glm::vec4 color) {

		auto itrp1 = points.insert(point1);
		if (itrp1.second) {
			indices.push_back((points.size() - 1));
		}
		else {
			indices.push_back((std::distance(points.begin(), itrp1.first)));
		}

		auto itrp2 = points.insert(point2);
		if (itrp2.second) {
			indices.push_back((points.size() - 1));
		}
		else {
			indices.push_back((std::distance(points.begin(), itrp2.first)));
		}

		auto itrp3 = points.insert(point3);
		if (itrp3.second) {
			indices.push_back((points.size() - 1));
		}
		else {
			indices.push_back((std::distance(points.begin(), itrp3.first)));
		}

		colors.push_back(color);
	}

};

static std::map<std::string, lines> line_plots;
static std::map<std::string, triangles> triangle_plots;

class ChartoGraphMessengerImpl : public ChartographMessenger::GraphPlotter::Service {
	grpc::Status CreatePlot(grpc::ServerContext* context, const ChartographMessenger::GraphData* request, ChartographMessenger::PlotResponse* response) override {
		std::string message("Plot Created");

		if (request->plotid().empty()) {
			return plotIdError(response);
		}

		if (request->graph_type() == ChartographMessenger::GraphType::LINE) {
			line_plots.insert({ request->plotid(), lines() });
		}
		else if (request->graph_type() == ChartographMessenger::GraphType::TRIANGLE) {
			triangle_plots.insert({ request->plotid(), triangles() });
		}
		else {
			return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Graph Type");
		}

		response->set_plotid(request->plotid());
		response->set_success(true);
		response->set_message(message);
		return grpc::Status::OK;
	}

	grpc::Status UpdatePlot(grpc::ServerContext* context, const ChartographMessenger::UpdateData* request, ChartographMessenger::PlotResponse* response) override {
		
		if (request->plotid().empty()) {
			return plotIdError(response);
		}

		if (request->has_new_data_points()) {
			for (ChartographMessenger::Point point : request->new_data_points().points()) {
				line_plots[request->plotid()].addPoint(glm::vec3(point.x(), point.y(), 0.0f));
			}
		}
		
		if (request->has_new_triangle()) {
			triangle_plots[request->plotid()].addTriangle(
				glm::vec3(request->new_triangle().p1().x(), request->new_triangle().p1().y(), 0.0f),
				glm::vec3(request->new_triangle().p2().x(), request->new_triangle().p2().y(), 0.0f),
				glm::vec3(request->new_triangle().p3().x(), request->new_triangle().p3().y(), 0.0f),
				glm::vec4(request->new_triangle().color().r(), request->new_triangle().color().g(), request->new_triangle().color().b(), request->new_triangle().color().a()));
		}

		if (request->has_new_triangles()) {
			for (ChartographMessenger::Triangle triangle : request->new_triangles().triangles()) {
				triangle_plots[request->plotid()].addTriangle(
					glm::vec3(triangle.p1().x(), triangle.p1().y(), 0.0f), 
					glm::vec3(triangle.p2().x(), triangle.p2().y(), 0.0f), 
					glm::vec3(triangle.p3().x(), triangle.p3().y(), 0.0f),
					glm::vec4(triangle.color().r(), triangle.color().g(), triangle.color().b(), triangle.color().a()));
			}
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

	inline grpc::Status graphTypeError(ChartographMessenger::PlotResponse* response) {
		return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Graph Type");
	}
};

static std::shared_ptr<grpc::Channel> serverChannel;

class ObjectLayer : public GUI::Layer {
public:
	ObjectLayer() : Layer("ObjectLayer") {}
	~ObjectLayer() {}

	void OnAttach() {}

	void OnDrawUpdate() {
		for (const auto& plot : line_plots) {
			Graphics::BatchRenderer::DrawLines(plot.second.points, plot.second.indices,glm::vec4(1.0));
		}

		for (const auto& plot : triangle_plots) {
			Graphics::BatchRenderer::DrawMesh(plot.second.points, plot.second.indices, plot.second.colors);
		}
	}

	void OnImGuiRender() {
		ImGui::Begin("Plots");
		for (auto& plot : line_plots) {
			if (ImGui::TreeNode(plot.first.c_str())) {
				if (ImGui::Button("Switch Index mode")) {
					plot.second.switchIndexMode();
				}
				ImGui::SameLine();
				ImGui::Text("%s", plot.second.getIndexMode() == CONTINUOUS ? "Continuous" : "Discrete");
				if (ImGui::Button("Remove")) {
					line_plots.erase(plot.first);
					ImGui::TreePop();
					break;
				}
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
