class Api::V1::BaseController < ActionController::API
  private
  def authenticate_device!
    token = request.authorization&.split("Bearer ")&.last
    expected = ENV["DEVICE_API_TOKEN"].to_s
    head :unauthorized and return if expected.blank? || token != expected
  end
end
